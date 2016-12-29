#include <QDebug>
#include <QDir>
#include <QtPlugin>
#include <QTimer>
#include <stdint.h>
#include "deconz/crc8.h"
#include "deconz/buffer_helper.h"
#include "std_otau_plugin.h"
#include "std_otau_widget.h"
#include "otau_file.h"
#include "otau_file_loader.h"
#include "otau_node.h"
#include "otau_model.h"
#include "deconz/aps_private.h"

#define VENDOR_DDEL 0x1135

#define IMG_TYPE_FLS_PP3_H3  0x0000
#define IMG_TYPE_FLS_NB      0x0002
#define IMG_TYPE_FLS_A2      0x0004
#define IMG_TYPE_FLS_H3      0x0008

#define MAX_RADIUS          0
#define MAX_ASDU_SIZE 49
/*
            U8  status
            U16 manufacturerCode;
            U16 imageType;
            U32 fileVersion;
            U32 offset;
            U8  dataSize
*/
#define IMAGE_BLOCK_RSP_HEADER_SIZE (1 + 2 + 2 + 4 + 4 + 1) // 14
#define ZCL_HEADER_SIZE (1 + 1 + 1) // frame control + seq + commandId
#define MAX_DATA_SIZE       (MAX_ASDU_SIZE - (ZCL_HEADER_SIZE + IMAGE_BLOCK_RSP_HEADER_SIZE))
#define MIN_RESPONSE_SPACING 20
#define MAX_RESPONSE_SPACING 500
#define DEFAULT_UPGRADE_TIME 5
#define CLEANUP_TIMER_DELAY  (3 * 60 * 1000)
#define CLEANUP_DELAY        (30 * 60 * 1000)
#define IMAGE_PAGE_TIMER_DELAY 5
#define ACTIVITY_TIMER_DELAY  1000
#define MAX_ACTIVITY   120 // hits 0 after 5 seconds
#define MAX_IMG_PAGE_REQ_RETRY   12
#define MAX_IMG_BLOCK_RSP_RETRY   10
#define WAIT_NEXT_REQUEST_TIMEOUT 20000
#define INVALID_APS_REQ_ID (0xff + 1) // request ids are 8-bit

#define OTAU_IMAGE_NOTIFY_CLID                 0x0201
#define OTAU_QUERY_NEXT_IMAGE_REQUEST_CLID     0x0202
#define OTAU_QUERY_NEXT_IMAGE_RESPONSE_CLID    0x8202
#define OTAU_IMAGE_BLOCK_REQUEST_CLID          0x0203
#define OTAU_IMAGE_BLOCK_RESPONSE_CLID         0x8203
#define OTAU_REPORT_STATUS_CLID                0x0205


#define IMG_BLK_RSP_HEADER_SIZE  (1 + 2 + 2 + 4 + 4)

#define DONT_CARE_FILE_VERSION                 0xFFFFFFFFUL

#define OTAU_IMAGE_TYPE_QJ             0x00 // Query jitter
#define OTAU_IMAGE_TYPE_QJ_MFC         0x01 // Query jitter, manufacturer code
#define OTAU_IMAGE_TYPE_QJ_MFC_IT      0x02 // Query jitter, manufacturer code, image type
#define OTAU_IMAGE_TYPE_QJ_MFC_IT_FV   0x03 // Query jitter, manufacturer code, image type, file version

static uint8_t notifyReqId;

/*! The constructor.
 */
StdOtauPlugin::StdOtauPlugin(QObject *parent) :
    QObject(parent)
{
    m_state = StateIdle;
    m_w = 0;
    m_srcEndpoint = 0x01; // TODO: ask from controller
    m_model = new OtauModel(this);
    m_imagePageTimer = new QTimer(this);

    m_imagePageTimer->setSingleShot(true);;
    m_imagePageTimer->setInterval(IMAGE_PAGE_TIMER_DELAY);

    connect(m_imagePageTimer, SIGNAL(timeout()),
            this, SLOT(imagePageTimerFired()));

    m_cleanupTimer = new QTimer(this);
    m_cleanupTimer->setSingleShot(true);
    m_cleanupTimer->setInterval(CLEANUP_TIMER_DELAY);

    connect(m_cleanupTimer, SIGNAL(timeout()),
            this, SLOT(cleanupTimerFired()));

    m_activityCounter = 0;
    m_activityTimer = new QTimer(this);
    m_activityTimer->setSingleShot(false);

    connect(m_activityTimer, SIGNAL(timeout()),
            this, SLOT(activityTimerFired()));

    //QString defaultImgPath = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation) + "/otau";
    QString defaultImgPath = deCONZ::getStorageLocation(deCONZ::HomeLocation) + "/otau";
    m_imgPath = deCONZ::appArgumentString("--otau-img-path", defaultImgPath);

    QDir otauDir(m_imgPath);

    if (otauDir.exists())
    {
        DBG_Printf(DBG_INFO, "Otau image path: %s\n", qPrintable(m_imgPath));
    }
    else
    {
        DBG_Printf(DBG_ERROR, "Otau image path does not exist: %s\n", qPrintable(m_imgPath));
    }

    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

    connect(apsCtrl, SIGNAL(apsdeDataConfirm(const deCONZ::ApsDataConfirm&)),
            this, SLOT(apsdeDataConfirm(const deCONZ::ApsDataConfirm&)));

    connect(apsCtrl, SIGNAL(apsdeDataIndication(const deCONZ::ApsDataIndication&)),
            this, SLOT(apsdeDataIndication(const deCONZ::ApsDataIndication&)));

    connect(apsCtrl, SIGNAL(nodeEvent(deCONZ::NodeEvent)),
            this, SLOT(nodeEvent(deCONZ::NodeEvent)));
}

/*! APSDE-DATA.indication callback.
    \param ind - the indication primitive
    \note Will be called from the main application for each incoming indication.
    Any filtering for nodes, profiles, clusters must be handled by this plugin.
 */
void StdOtauPlugin::apsdeDataIndication(const deCONZ::ApsDataIndication &ind)
{
    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();
    if (!apsCtrl)
    {
        return;
    }

//    if (ind.profileId() == ZDP_PROFILE_ID && ind.clusterId() == ZDP_MATCH_DESCRIPTOR_CLID)
//    {
//        matchDescriptorRequest(ind);
//    }

    if (ind.clusterId() != OTAU_CLUSTER_ID)
    {
        return;
    }

    deCONZ::ZclFrame zclFrame;

    QDataStream stream(ind.asdu());
    stream.setByteOrder(QDataStream::LittleEndian);
    zclFrame.readFromStream(stream);

    // filter
    if (zclFrame.isClusterCommand())
    {
        switch (zclFrame.commandId())
        {
        case OTAU_QUERY_NEXT_IMAGE_REQUEST_CMD_ID:
        case OTAU_IMAGE_BLOCK_REQUEST_CMD_ID:
        case OTAU_IMAGE_PAGE_REQUEST_CMD_ID:
        case OTAU_UPGRADE_END_REQUEST_CMD_ID:
            m_cleanupTimer->stop();
            m_cleanupTimer->start();
            break;

        default:
            return;
        }
    }
    else
    {
        if (zclFrame.commandId() == deCONZ::ZclDefaultResponseId)
        {
            switch (zclFrame.defaultResponseCommandId())
            {
            case OTAU_QUERY_NEXT_IMAGE_REQUEST_CMD_ID:
            case OTAU_QUERY_NEXT_IMAGE_RESPONSE_CMD_ID:
            case OTAU_IMAGE_BLOCK_REQUEST_CMD_ID:
            case OTAU_IMAGE_BLOCK_RESPONSE_CMD_ID:
            case OTAU_IMAGE_PAGE_REQUEST_CMD_ID:
            case OTAU_UPGRADE_END_REQUEST_CMD_ID:
            case OTAU_UPGRADE_END_RESPONSE_CMD_ID:
                DBG_Printf(DBG_INFO, "otau default rsp cmd: 0x%02X, status 0x%02X\n", zclFrame.defaultResponseCommandId(), (uint8_t)zclFrame.defaultResponseStatus());
                break;

            default:
                break;
            }
            return;
        }
    }

    bool create = true;
    OtauNode *node = m_model->getNode(ind.srcAddress(), create);

    if (!node)
    {
        return;
    }

    node->lastActivity.restart();
    if (!zclFrame.isDefaultResponse())
    {
        node->setLastZclCommand(zclFrame.commandId());
    }

    // filter
    if (zclFrame.isClusterCommand())
    {
        switch (zclFrame.commandId())
        {
        case OTAU_QUERY_NEXT_IMAGE_REQUEST_CMD_ID:
            queryNextImageRequest(ind, zclFrame);
            break;

        case OTAU_IMAGE_BLOCK_REQUEST_CMD_ID:
            imageBlockRequest(ind, zclFrame);
            break;

        case OTAU_IMAGE_PAGE_REQUEST_CMD_ID:
            imagePageRequest(ind, zclFrame);
            break;

        case OTAU_UPGRADE_END_REQUEST_CMD_ID:
            upgradeEndRequest(ind, zclFrame);
            break;

        default:
            break;
        }
    }

    m_model->nodeDataUpdate(node);
}

/*! APSDE-DATA.confirm callback.
    \param conf - the confirm primitive
    \note Will be called from the main application for each incoming confirmation,
    even if the APSDE-DATA.request was not issued by this plugin.
 */
void StdOtauPlugin::apsdeDataConfirm(const deCONZ::ApsDataConfirm &conf)
{
    if (m_state == StateNotify)
    {
        if (conf.id() == notifyReqId)
        {
            setState(StateIdle);
        }
    }
    else if (m_state == StateIdle)
    {
        OtauNode *node = m_model->getNode(conf.dstAddress());

        if (node)
        {
            if (node->state() == OtauNode::NodeAbort)
            {
                return;
            }

            if (node->apsRequestId == INVALID_APS_REQ_ID)
            { }
            else if (node->apsRequestId == conf.id())
            {
                node->apsRequestId = INVALID_APS_REQ_ID;

                // wait for next query
                if (conf.status() != deCONZ::ApsSuccessStatus)
                {
                    DBG_Printf(DBG_INFO, "otau aps conf failed status 0x%02X\n", conf.status());
                    node->setState(OtauNode::NodeError);
                }
                else
                {
                    node->refreshTimeout();
                }

                if (node->lastZclCmd() == OTAU_IMAGE_PAGE_REQUEST_CMD_ID)
                {
                    //imagePageResponse(node);
                }
                else if (node->zclCommandId == OTAU_UPGRADE_END_RESPONSE_CMD_ID)
                {
                    if (conf.status() == deCONZ::ApsSuccessStatus)
                    {
                        node->setHasData(false);
//                        invalidateUpdateEndRequest(node);
                    }
                }
            }
        }
    }
}

/*! Handler for node events.
    \param event - the event which occured
 */
void StdOtauPlugin::nodeEvent(const deCONZ::NodeEvent &event)
{
    if (!event.node())
    {
        return;
    }

    if (event.event() == deCONZ::NodeEvent::UpdatedSimpleDescriptor)
    {
        checkIfNewOtauNode(event.node(), event.endpoint());
    }
}

/*! Checks if a new otau image for the node is available in the otau folder.
    Otau images must be in the <otau> directory and must have a proper formatted filename.
    All numbers are hexaecimal and in capital letters.
    filename: <manufacturer code>-<imagetype>-<fileversion>-someArbitraryText.zigbee
    example: 113D-AB12-1F010400-FLS-RGB.zigbee

    \param node - the node for which the check will be done
    \param path - the path to look for .zigbee files
 */
bool StdOtauPlugin::checkForUpdateImageImage(OtauNode *node, const QString &path)
{
    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();
    if (!apsCtrl)
    {
        return false;
    }

    if (apsCtrl->getParameter(deCONZ::ParamOtauActive) == 0)
    {
        return false;
    }

    bool ok;
    uint32_t cmpFileVersion = node->softwareVersion();
    uint32_t fileVersion;
    uint16_t imageType;
    uint16_t manufacturerId;
    QString updateFile = "";
    QDir dir(path);

    if (!dir.exists())
    {
        DBG_Printf(DBG_INFO, "Otau image path does not exist: %s\n", qPrintable(path));
        return false;
    }

    QStringList ls = dir.entryList();

    QStringList::const_iterator i = ls.begin();
    QStringList::const_iterator end = ls.end();

    for (; i != end; ++i)
    {
        if (!i->endsWith(".zigbee"))
        {
            continue;
        }

        QString plain = *i;
        plain.replace(".zigbee", "");

        QStringList args = plain.split('-');

        if (args.size() >= 3)
        {
            manufacturerId = args[0].toUShort(&ok, 16);

            if (!ok || manufacturerId != node->manufacturerId)
            {
                continue;
            }


            imageType = args[1].toUShort(&ok, 16);

            if (!ok)
            {
                continue;
            }

            if (imageType == node->imageType())
            {
                fileVersion = args[2].toUInt(&ok, 16);

                if (!ok)
                {
                    continue;
                }

                if (fileVersion > cmpFileVersion)
                {
                    updateFile = *i;
                    cmpFileVersion = fileVersion;
                    DBG_Printf(DBG_INFO, "Match otau version 0x%08X image type 0x%04X\n", fileVersion, imageType);
                }
            }

        }
    }

    if (!updateFile.isEmpty())
    {
        updateFile.prepend(path + "/");
        OtauFileLoader ld;

        if (ld.readFile(updateFile, node->file))
        {
            node->setHasData(true);
            DBG_Printf(DBG_INFO, "Found update file %s\n", qPrintable(updateFile));
        }
        else
        {
            node->setHasData(false);
            DBG_Printf(DBG_INFO, "Found invalid update file %s\n", qPrintable(updateFile));
        }
    }

    return false;
}

/*! Invalidates the upgrade end request.
 */
void StdOtauPlugin::invalidateUpdateEndRequest(OtauNode *node)
{
    if (node)
    {
        if ((node->upgradeEndReq.fileVersion != 0) || (node->upgradeEndReq.manufacturerCode != 0))
        {
            DBG_Printf(DBG_INFO, "otau invalide update end request for node %s\n", qPrintable(node->address().toStringExt()));
        }

        node->upgradeEndReq.status = 0;
        node->upgradeEndReq.manufacturerCode = 0;
        node->upgradeEndReq.fileVersion = 0;
        node->upgradeEndReq.imageType = 0;
    }
}

/*! Timer callback to send a delayed image notify request.
 */
void StdOtauPlugin::delayedImageNotify()
{
    unicastImageNotify(m_delayedImageNotifyAddr);
    m_delayedImageNotifyAddr.clear();
}

/*! Handler to automatically send image page responses.
 */
void StdOtauPlugin::imagePageTimerFired()
{
    if (!m_model || m_model->nodes().empty())
    {
        return;
    }

    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();
    if (!apsCtrl)
    {
        return;
    }

    if (apsCtrl->getParameter(deCONZ::ParamOtauActive) == 0)
    {
        return;
    }

    std::vector<OtauNode*>::iterator i = m_model->nodes().begin();
    std::vector<OtauNode*>::iterator end = m_model->nodes().end();

    bool refire = false;

    for (; i != end; ++i)
    {
        DBG_Assert(*i != 0);
        OtauNode *node = *i;
        if (!node)
            continue;

        if (node->state() == OtauNode::NodeWaitPageSpacing)
        {
            refire = true;
            if (!imagePageResponse(node))
            {
                if (node->imgBlockResponseRetry >= MAX_IMG_BLOCK_RSP_RETRY)
                {
                    // giveup
                    node->setState(OtauNode::NodeIdle);
                }
            }
        }
        else if (node->state() == OtauNode::NodeWaitNextRequest)
        {
            refire = true;

            if (node->lastActivity.hasExpired(WAIT_NEXT_REQUEST_TIMEOUT))
            {
                node->imgPageRequestRetry++;
                if (node->imgPageRequestRetry >= MAX_IMG_PAGE_REQ_RETRY)
                {
                    // giveup
                    node->setState(OtauNode::NodeIdle);
                }
                else
                {
                    DBG_Printf(DBG_INFO, "otau resend page\n");

                    node->lastActivity.restart();

                    node->imgPageReq.pageBytesDone = 0;
                    node->imgBlockReq = node->imgPageReq;

                    node->setOffset(node->imgBlockReq.offset);
                    node->setImageType(node->imgBlockReq.imageType);

                    node->imgPageRequestRetry = 0;
                    node->imgBlockResponseRetry = 0;

                    node->setState(OtauNode::NodeWaitPageSpacing);
                }
            }
        }
    }

    if (refire && !m_imagePageTimer->isActive())
    {
        m_imagePageTimer->start(IMAGE_PAGE_TIMER_DELAY);
    }
}

/*! Handler to cleanup timed out nodes.
 */
void StdOtauPlugin::cleanupTimerFired()
{
    if (!m_model)
    {
        return;
    }

    int activeNodes = 0;

    std::vector<OtauNode*>::iterator i = m_model->nodes().begin();
    std::vector<OtauNode*>::iterator end = m_model->nodes().end();

    for (; i != end; ++i)
    {
        OtauNode *node = *i;
        if (node->hasData())
        {
            if (node->lastActivity.hasExpired(CLEANUP_DELAY))
            {
                node->file.subElements.clear();
                node->setHasData(false);
                DBG_Printf(DBG_INFO, "otau cleanup node\n");
            }
            else
            {
                activeNodes++;
            }
        }
    }

    if (activeNodes)
    {
        m_cleanupTimer->start();
    }
}

void StdOtauPlugin::activityTimerFired()
{
    if (m_activityCounter > 0)
    {
        m_activityCounter--;
    }

    if (m_activityCounter == 0)
    {
        m_activityTimer->stop();
    }
    else if (m_activityCounter < 0)
    { // sanity
        m_activityCounter = 0;
    }
}

void StdOtauPlugin::markOtauActivity(const deCONZ::Address &address)
{
    if (0 == m_activityCounter || !m_activityTimer->isActive() || address.ext() == m_activityAddress.ext())
    {
        m_activityCounter = MAX_ACTIVITY;
        m_activityAddress = address;

        if (!m_activityTimer->isActive())
        {
            m_activityTimer->start(ACTIVITY_TIMER_DELAY);
        }
    }
}

/*! Sends a image notify request.
    \param notf - the request parameters
    \return true on success false otherwise
 */
bool StdOtauPlugin::imageNotify(ImageNotifyReq *notf)
{
    if (m_state == StateIdle)
    {
        deCONZ::ApsDataRequest req;
        deCONZ::ZclFrame zclFrame;

        OtauNode *node = m_model->getNode(notf->addr);

        req.setDstAddressMode(notf->addrMode);
        req.dstAddress() = notf->addr;
        req.setDstEndpoint(notf->dstEndpoint);
        req.setSrcEndpoint(m_srcEndpoint);
        if (node)
        {
            req.setProfileId(node->profileId);
            DBG_Printf(DBG_INFO, "send img notify to %s\n", qPrintable(node->address().toStringExt()));
        }
        else
        {
            req.setProfileId(0x0104);
        }
        req.setClusterId(OTAU_CLUSTER_ID);

        req.setRadius(notf->radius);

        zclFrame.setSequenceNumber(m_zclSeq++);
        zclFrame.setCommandId(OTAU_IMAGE_NOTIFY_CMD_ID);

        uint8_t frameControl = deCONZ::ZclFCClusterCommand |
                deCONZ::ZclFCDirectionServerToClient;

        if (notf->addr.isNwkBroadcast())
        {
            frameControl |= deCONZ::ZclFCDisableDefaultResponse;
        }

        zclFrame.setFrameControl(frameControl);

        { // ZCL payload
            QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            stream << (uint8_t)0x00; // query jitter
            stream << (uint8_t)100; // query jitter value
        }

        { // ZCL frame
            QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);
            zclFrame.writeToStream(stream);
        }

        if (deCONZ::ApsController::instance()->apsdeDataRequest(req) == deCONZ::Success)
        {
            return true;
        }
    }

    return false;
}

/*! Broadcasts a image notify request.
 */
bool StdOtauPlugin::broadcastImageNotify()
{
    ImageNotifyReq notf;

    notf.radius = 0;
    notf.addr.setNwk(deCONZ::BroadcastRxOnWhenIdle);
    notf.addrMode = deCONZ::ApsNwkAddress;
    notf.dstEndpoint = 0xFF; // broadcast endpoint

    return imageNotify(&notf);
}

/*! Sends a image notify request per unicast.
    \param addr - the destination address
    \return true on success false otherwise
 */
bool StdOtauPlugin::unicastImageNotify(const deCONZ::Address &addr)
{
    if (addr.hasExt())
    {
        ImageNotifyReq notf;

        OtauNode *node = m_model->getNode(addr);

        if (!node)
        {
            return false;
        }

        // blacklist some faulty versions tue image notify bug in BitCloud 3.2, 3.3
        if (node->manufacturerId == VENDOR_DDEL)
        {
            if (node->imageType() == IMG_TYPE_FLS_PP3_H3)
            {
                if (node->softwareVersion() < 0x201000C4)
                {
                    return false;
                }
            }
            else if (node->imageType() == IMG_TYPE_FLS_A2)
            {
                if (node->softwareVersion() < 0x201000C4)
                {
                    return false;
                }
            }
            else if (node->imageType() == IMG_TYPE_FLS_NB)
            {
                if (node->softwareVersion() < 0x200000C8)
                {
                    return false;
                }
            }
        }

        notf.radius = 0;
        notf.addr = addr;
        notf.addrMode = deCONZ::ApsExtAddress;
        notf.dstEndpoint = node->endpoint;

        return imageNotify(&notf);
    }

    return false;
}

/*! Sends a upgrade end request to a node via unicast.
    \param addr - the destination address
 */
void StdOtauPlugin::unicastUpgradeEndRequest(const deCONZ::Address &addr)
{
    if (addr.hasExt())
    {
        OtauNode *node = m_model->getNode(addr);

        DBG_Assert(node != 0);
        if (node)
        {
            if (!upgradeEndResponse(node, DEFAULT_UPGRADE_TIME))
            {
                DBG_Printf(DBG_INFO, "otau failed to send upgrade end response\n");
            }
        }
    }
}

/*! Aborts searching for otau nodes.
 */
bool StdOtauPlugin::abortSearch()
{
    if (m_state == StateNotify)
    {
        setState(StateIdle);
        return true;
    }

    return false;
}

/*! Handles a match descriptor request and sends the response if needed.
    \param ind - the APSDE-DATA.indication
 */
void StdOtauPlugin::matchDescriptorRequest(const deCONZ::ApsDataIndication &ind)
{

    bool sendResponse = false;
    uint8_t seqNo;
    uint16_t shortAddr;
    uint16_t profileId;
    uint8_t serverClusterCount;
    //uint8_t serverClusterCount; // not used

    if (ind.asdu().size() < 7) // minimum size for match descriptor request
    {
        DBG_Printf(DBG_INFO, "otau ignore match descriptor req from 0x%04X with asduSize %d\n", ind.srcAddress().nwk(), ind.asdu().size());
    }

    {
        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);
        stream >> seqNo;
        stream >> shortAddr;
        stream >> profileId;
        stream >> serverClusterCount;

        for (uint i = 0; i < serverClusterCount; i++)
        {
            uint16_t clusterId;
            stream >> clusterId;

            if (clusterId == OTAU_CLUSTER_ID)
            {
                DBG_Printf(DBG_INFO, "otau match descriptor req, profileId 0x%04X from 0x%04X\n", profileId, ind.srcAddress().nwk());
                sendResponse = true;
                break;
            }
        }
    }

    if (sendResponse)
    {
        deCONZ::ApsDataRequest req;

        req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
        req.dstAddress() = ind.srcAddress();
        req.setDstAddressMode(deCONZ::ApsNwkAddress);
        req.setProfileId(ZDP_PROFILE_ID);
        req.setClusterId(ZDP_MATCH_DESCRIPTOR_RSP_CLID);

        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status = 0x00; // success
        shortAddr = 0x0000; // TODO query from apsController
        uint8_t matchLength = 0x01;
        uint8_t matchList = m_srcEndpoint;

        stream << seqNo;
        stream << status;
        stream << shortAddr;
        stream << matchLength;
        stream << matchList;

        if (deCONZ::ApsController::instance()->apsdeDataRequest(req) == deCONZ::Success)
        {
            DBG_Printf(DBG_INFO, "otau send match descriptor rsp, match endpoint 0x%02X\n", matchList);
        }
        else
        {
            DBG_Printf(DBG_INFO, "otau send match descriptor rsp failed\n");
        }
    }
}

/*! Handles a query next image request and sends the response.
    \param ind - the APSDE-DATA.indication
    \param zclFrame - the ZCL frame
 */
void StdOtauPlugin::queryNextImageRequest(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    uint16_t u16;
    uint32_t u32;
    OtauNode *node = m_model->getNode(ind.srcAddress());

    if (!node)
    {
        DBG_Printf(DBG_INFO, "otau query next image request for unknown node %s\n", qPrintable(ind.srcAddress().toStringExt()));
        return;
    }

    if ((zclFrame.payload().size() != 9) &&
        (zclFrame.payload().size() != 11)) // with hardware version present
    {
        DBG_Printf(DBG_INFO, "otau query next image request for node %s invalid payload length %d\n", qPrintable(ind.srcAddress().toStringExt()), zclFrame.payload().size());
        return;
    }

    invalidateUpdateEndRequest(node);

    node->reqSequenceNumber = zclFrame.sequenceNumber();
    node->endpoint = ind.srcEndpoint();
    node->profileId = ind.profileId();
    node->setAddress(ind.srcAddress());
    node->refreshTimeout();
    node->restartElapsedTimer();
    node->setStatus(OtauNode::StatusSuccess);

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    uint8_t fieldControl;

    stream >> fieldControl;
    stream >> node->manufacturerId;
    stream >> u16;
    node->setImageType(u16);
    stream >> u32;
    node->setSoftwareVersion(u32);
    if (fieldControl & 0x01)
    {
        stream >> u16;
        node->setHardwareVersion(u16);
    }
    else
    {
        node->setHardwareVersion(0xFFFF);
    }

    DBG_Printf(DBG_INFO, "otau query next img req: %s mfCode: 0x%04X, img type: 0x%04X, sw version: 0x%08X\n",
               qPrintable(ind.srcAddress().toStringExt()), node->manufacturerId, node->imageType(), node->softwareVersion());

    if (deCONZ::ApsController::instance()->getParameter(deCONZ::ParamOtauActive) == 0)
    {
        return;
    }

    // check for image
    if (!node->hasData())
    {
        node->file.subElements.clear();
        node->setHasData(false);
        node->setPermitUpdate(false);

        if (!checkForUpdateImageImage(node, m_imgPath))
        {
            QString secondaryPath = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation) + "/otau";

            if (!checkForUpdateImageImage(node, secondaryPath))
            {

            }
        }
    }

    if (node->hasData())
    {
        node->setPermitUpdate(true);
    }

    if (queryNextImageResponse(node))
    {
        node->setState(OtauNode::NodeBusy);
    }
    else
    {
        // TODO handle error
        node->setState(OtauNode::NodeIdle);
    }
}

/*! Sends a query next image response.
    \param node - the destination node
    \return true on success false otherwise
 */
bool StdOtauPlugin::queryNextImageResponse(OtauNode *node)
{
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    DBG_Assert(node->address().hasExt());
    if (!node->address().hasExt())
    {
        return false;
    }

    req.setProfileId(node->profileId);
    req.setDstEndpoint(node->endpoint);
    req.setClusterId(OTAU_CLUSTER_ID);
    req.dstAddress().setExt(node->address().ext());
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.setSrcEndpoint(m_srcEndpoint);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.setRadius(MAX_RADIUS);

    zclFrame.setSequenceNumber(node->reqSequenceNumber);
    zclFrame.setCommandId(OTAU_QUERY_NEXT_IMAGE_RESPONSE_CMD_ID);

    zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // ZCL payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        if (node->state() == OtauNode::NodeAbort)
        {
            stream << (uint8_t)OTAU_ABORT;
            DBG_Printf(DBG_INFO, "Send query next image response: OTAU_ABORT\n");
        }
//        else if (!otauIsActive())
//        {
//            stream << (uint8_t)OTAU_NO_IMAGE_AVAILABLE;
//            DBG_Printf(DBG_INFO, "Send query next image response: OTAU_NO_IMAGE_AVAILABLE\n");
//        }
        else if (otauIsActive() && (m_activityAddress.ext() != node->address().ext()))
        {
            DBG_Printf(DBG_INFO, "Busy, don't answer and let node run in timeout\n");
            return false;
        }
        else if (node->manufacturerId == VENDOR_DDEL &&
                 node->imageType() == IMG_TYPE_FLS_PP3_H3 &&
                 node->softwareVersion() >= 0x20000050 &&
                 node->softwareVersion() <= 0x20000054)
        {
            // FIXME workaround to prevent update FLS-H lp
            stream << (uint8_t)OTAU_NO_IMAGE_AVAILABLE;
            DBG_Printf(DBG_INFO, "Send query next image response: OTAU_NO_IMAGE_AVAILABLE to FLS-H lp\n");
        }
        else if (node->permitUpdate() && node->hasData())
        {
            node->rawFile = node->file.toArray();
            stream << (uint8_t)OTAU_SUCCESS;
            stream << node->file.manufacturerCode;
            stream << node->file.imageType;
            stream << node->file.fileVersion;
            stream << node->file.totalImageSize;

            markOtauActivity(node->address());
            if (node->address().ext() == m_activityAddress.ext())
            {
                m_activityCounter = 5;
            }
        }
        else
        {
            stream << (uint8_t)OTAU_NO_IMAGE_AVAILABLE;
            DBG_Printf(DBG_INFO, "Send query next image response: OTAU_NO_IMAGE_AVAILABLE\n");
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (deCONZ::ApsController::instance()->apsdeDataRequest(req) == 0)
    {
        node->apsRequestId = req.id();
        node->zclCommandId = zclFrame.commandId();
        return true;
    }

    return false;
}

/*! Handles a image block request and sends the response.
    \param ind - the APSDE-DATA.indication
    \param zclFrame - the ZCL frame
 */
void StdOtauPlugin::imageBlockRequest(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    OtauNode *node = m_model->getNode(ind.srcAddress());

    if (!node)
    {
        return;
    }

    markOtauActivity(node->address());

    if (otauIsActive() && node->address().ext() != m_activityAddress.ext())
    {
        return; // ignore
    }

    node->refreshTimeout();
    invalidateUpdateEndRequest(node);

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> node->imgBlockReq.fieldControl;
    stream >> node->imgBlockReq.manufacturerCode;
    stream >> node->imgBlockReq.imageType;
    stream >> node->imgBlockReq.fileVersion;
    stream >> node->imgBlockReq.offset;
    stream >> node->imgBlockReq.maxDataSize;

    if (node->imgBlockReq.fileVersion == DONT_CARE_FILE_VERSION)
    {
        node->imgBlockReq.fileVersion = node->file.fileVersion;
    }

    node->setStatus(OtauNode::StatusSuccess);
    node->setOffset(node->imgBlockReq.offset);
    node->setImageType(node->imgBlockReq.imageType);
    node->notifyElapsedTimer();

    node->reqSequenceNumber = zclFrame.sequenceNumber();
    node->endpoint = ind.srcEndpoint();
    node->profileId = ind.profileId();

    DBG_Printf(DBG_INFO, "Img block req fwVersion:0x%08X, offset: 0x%08X, maxsize: %u\n", node->imgBlockReq.fileVersion, node->imgBlockReq.offset, node->imgBlockReq.maxDataSize);

    // IEEE address present?
    if (node->imgBlockReq.fieldControl & 0x01)
    {
        quint64 extAddr;
        stream >> extAddr;

        deCONZ::Address addr = node->address();
        addr.setExt(extAddr);
        node->setAddress(addr);
    }

    if (imageBlockResponse(node))
    {
        node->setState(OtauNode::NodeBusy);
    }
    else
    {
        DBG_Printf(DBG_INFO, "otau failed to send image block response\n");
        node->setState(OtauNode::NodeIdle);
    }
}

/*! Sends a image block response.
    \param node - the destination node
    \return true on success false otherwise
 */
bool StdOtauPlugin::imageBlockResponse(OtauNode *node)
{
    uint8_t dataSize = 0;
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    DBG_Assert(node->address().hasExt());
    if (!node->address().hasExt())
    {
        return false;
    }

    req.setProfileId(node->profileId);
    req.setDstEndpoint(node->endpoint);
    req.setClusterId(OTAU_CLUSTER_ID);
    req.dstAddress() = node->address();
    req.setDstAddressMode(deCONZ::ApsExtAddress);

    req.setSrcEndpoint(m_srcEndpoint);
    // APS ACKs are enabled for single image block requests
    // they are disabled for image page request responses
    if ((node->lastZclCmd() == OTAU_IMAGE_BLOCK_REQUEST_CMD_ID) || (node->state() == OtauNode::NodeAbort))
    {
        req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    }
    else
    {
        if (m_w->acksEnabled())
        {
            req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
        }
        else
        {
        }

        req.setState(deCONZ::FireAndForgetState);
    }

    zclFrame.setSequenceNumber(node->reqSequenceNumber);
    req.setRadius(MAX_RADIUS);

    zclFrame.setCommandId(OTAU_IMAGE_BLOCK_RESPONSE_CMD_ID);

    zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // ZCL payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        if ((node->imgBlockReq.fileVersion != node->file.fileVersion) ||
            (node->imgBlockReq.imageType != node->file.imageType) ||
            (node->imgBlockReq.manufacturerCode != node->file.manufacturerCode))
        {
            stream << (uint8_t)OTAU_ABORT;
            node->setState(OtauNode::NodeAbort);
        }
        else if (node->state() == OtauNode::NodeAbort)
        {
            stream << (uint8_t)OTAU_ABORT;
        }
        else if (!node->permitUpdate() || !node->hasData())
        {
            stream << (uint8_t)OTAU_NO_IMAGE_AVAILABLE;
        }
        else if (node->imgBlockReq.offset < (uint32_t)node->rawFile.size())
        {
            if (node->imgBlockReq.maxDataSize > MAX_DATA_SIZE)
            {
                dataSize = MAX_DATA_SIZE;
            }
            else
            {
                dataSize = node->imgBlockReq.maxDataSize;
            }

            uint32_t offset = node->imgBlockReq.offset;

            stream << (uint8_t)OTAU_SUCCESS;
            stream << node->file.manufacturerCode;
            stream << node->file.imageType;
            stream << node->file.fileVersion;
            stream << node->imgBlockReq.offset;

            dataSize = (uint8_t)qMin((uint32_t)dataSize, ((uint32_t)node->rawFile.size() - offset));

            if (node->lastZclCmd() == OTAU_IMAGE_PAGE_REQUEST_CMD_ID)
            {
                // only fill till page boundary
                dataSize = qMin((uint32_t)dataSize, (uint32_t)(node->imgBlockReq.pageSize - node->imgBlockReq.pageBytesDone));

                if (dataSize == 0)
                {
                    // dont send empty block response
                    return false;
                }

                node->imgBlockReq.pageBytesDone += dataSize;
                node->imgBlockReq.offset += dataSize;
            }

            // truncate datasize if not enough data is left
            uint32_t avail = (uint32_t)node->rawFile.size() - offset;
            if (avail < dataSize)
            {
                dataSize = avail;
            }

            stream << dataSize;

            for (uint i = 0; (i < dataSize) && (offset < (uint32_t)node->rawFile.size()); i++)
            {
                stream << (uint8_t)node->rawFile[offset++];
            }
        }
        else
        {
            stream << (uint8_t)OTAU_MALFORMED_COMMAND;
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (deCONZ::ApsController::instance()->apsdeDataRequest(req) == 0)
    {
        node->apsRequestId = req.id();
        node->zclCommandId = zclFrame.commandId();
        node->lastResponseTime.restart();
        return true;
    }
    else
    {
        // revert
        if (node->lastZclCmd() == OTAU_IMAGE_PAGE_REQUEST_CMD_ID)
        {
            node->imgBlockReq.pageBytesDone -= dataSize;
            node->imgBlockReq.offset -= dataSize;
        }
    }

    return false;
}

/*! Handles a image page request and sends the response.
    \param ind - the APSDE-DATA.indication
    \param zclFrame - the ZCL frame
 */
void StdOtauPlugin::imagePageRequest(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    OtauNode *node = m_model->getNode(ind.srcAddress());

    if (!node)
    {
        return;
    }

    markOtauActivity(node->address());

    if (otauIsActive() && node->address().ext() != m_activityAddress.ext())
    {
        return; // ignore
    }

    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();
    if (!apsCtrl)
    {
        return;
    }

    node->reqSequenceNumber = zclFrame.sequenceNumber();

    if (node->state() == OtauNode::NodeAbort)
    {
        defaultResponse(node, OTAU_ABORT);
        return;
    }

    if (!m_w->pageRequestEnabled())
    {
        defaultResponse(node, OTAU_UNSUP_CLUSTER_COMMAND);
        return;
    }

    node->refreshTimeout();
    invalidateUpdateEndRequest(node);

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> node->imgPageReq.fieldControl;
    stream >> node->imgPageReq.manufacturerCode;
    stream >> node->imgPageReq.imageType;
    stream >> node->imgPageReq.fileVersion;
    stream >> node->imgPageReq.offset;
    stream >> node->imgPageReq.maxDataSize;
    stream >> node->imgPageReq.pageSize;
    stream >> node->imgPageReq.responseSpacing;

    if (node->imgPageReq.fileVersion == DONT_CARE_FILE_VERSION)
    {
        node->imgPageReq.fileVersion = node->file.fileVersion;
    }

    if (node->imgPageReq.responseSpacing > MAX_RESPONSE_SPACING)
    {
        node->imgPageReq.responseSpacing = MAX_RESPONSE_SPACING;
    }
    else if (node->imgPageReq.responseSpacing < MIN_RESPONSE_SPACING)
    {
        node->imgPageReq.responseSpacing = MIN_RESPONSE_SPACING;
    }

    node->imgPageReq.pageBytesDone = 0;

    node->imgBlockReq = node->imgPageReq;

    node->setOffset(node->imgBlockReq.offset);
    node->setImageType(node->imgBlockReq.imageType);
    node->notifyElapsedTimer();

    node->endpoint = ind.srcEndpoint();
    node->profileId = ind.profileId();

    if (DBG_IsEnabled(DBG_INFO_L2))
    {
        DBG_Printf(DBG_INFO_L2, "otau img page req fwVersion:0x%08X, offset: 0x%08X, pageSize: %u, rspSpacing: %u ms\n", node->imgBlockReq.fileVersion, node->imgBlockReq.offset, node->imgBlockReq.pageSize, node->imgBlockReq.responseSpacing);
    }

    // IEEE address present?
    if (node->imgBlockReq.fieldControl & 0x01)
    {
        quint64 requestNodeAddress;
        stream >> requestNodeAddress;
        // not used
    }

    node->imgPageRequestRetry = 0;
    node->imgBlockResponseRetry = 0;

    node->setState(OtauNode::NodeWaitPageSpacing);
    if (!m_imagePageTimer->isActive())
        m_imagePageTimer->start(IMAGE_PAGE_TIMER_DELAY);
}

/*! Sends a image block responses for a whole page.
    \param node - the destination node
    \return true on success false otherwise
 */
bool StdOtauPlugin::imagePageResponse(OtauNode *node)
{
    DBG_Assert(node != 0);
    if (!node)
    {
        return false;
    }

    if (node->lastZclCmd() != OTAU_IMAGE_PAGE_REQUEST_CMD_ID)
    {
        return false;
    }

    if (node->state() == OtauNode::NodeAbort)
    {
        return imageBlockResponse(node);
    }

    if (node->imgBlockReq.pageBytesDone >= node->imgBlockReq.pageSize)
    {
        node->setState(OtauNode::NodeWaitNextRequest);

        if (!m_imagePageTimer->isActive())
            m_imagePageTimer->start(IMAGE_PAGE_TIMER_DELAY);
        return true;
    }

    if (node->imgBlockReq.pageBytesDone > 0)
    {
        quint16 spacing = node->imgBlockReq.responseSpacing;

        if (m_w->packetSpacingEnabled())
        {
            spacing = m_w->packetSpacingMs();
        }

        if (node->lastResponseTime.isValid() &&
            !node->lastResponseTime.hasExpired(spacing))
        {
            node->setState(OtauNode::NodeWaitPageSpacing);

            if (!m_imagePageTimer->isActive())
                m_imagePageTimer->start(IMAGE_PAGE_TIMER_DELAY);

            return true;
        }
    }


    int succ = 0;
    for (int i = 0; i < 1; i++)
    {
        if ((int)node->imgBlockReq.offset >= node->rawFile.size())
        {
            node->setState(OtauNode::NodeWaitNextRequest);
            return true;
        }

        if (imageBlockResponse(node))
        {
//            if (node->state() == OtauNode::NodeAbort)
//            {
//                return false;
//            }

            node->imgBlockResponseRetry = 0;
            succ++;
        }
        else
        {
            node->setState(OtauNode::NodeWaitPageSpacing);
            node->imgBlockResponseRetry++;
            DBG_Printf(DBG_INFO, "otau img block response failed\n");
            break;
        }
    }

    return succ > 0;
}

/*! Handles a upgrade end request and sends the response.
    \param ind - the APSDE-DATA.indication
    \param zclFrame - the ZCL frame
 */
void StdOtauPlugin::upgradeEndRequest(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    OtauNode *node = m_model->getNode(ind.srcAddress());

    if (!node)
    {
        return;
    }

    node->refreshTimeout();

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> node->upgradeEndReq.status;
    stream >> node->upgradeEndReq.manufacturerCode;
    stream >> node->upgradeEndReq.imageType;
    stream >> node->upgradeEndReq.fileVersion;

    if (node->hasData())
    {
        node->setOffset(node->imgBlockReq.offset);
        node->setImageType(node->imgBlockReq.imageType);
    }
    node->notifyElapsedTimer();

    node->reqSequenceNumber = zclFrame.sequenceNumber();
    node->endpoint = ind.srcEndpoint();
    node->profileId = ind.profileId();

    DBG_Printf(DBG_INFO, "otau upgrade end req: status: 0x%02X, fwVersion:0x%08X, imgType: 0x%04X\n", node->upgradeEndReq.status, node->upgradeEndReq.fileVersion, node->upgradeEndReq.imageType);

    node->setState(OtauNode::NodeIdle);


    if (m_activityAddress.ext() == node->address().ext())
    {
        m_activityCounter = 1;
    }

    if (node->upgradeEndReq.status == OTAU_SUCCESS)
    {
        node->setStatus(OtauNode::StatusSuccess);
        node->setOffset(node->file.totalImageSize); // mark done

        // use time from widget
        uint32_t upgradeTime = m_w->restartTime();

        if (!upgradeEndResponse(node, upgradeTime))
        {
            DBG_Printf(DBG_INFO, "otau failed to send upgrade end response\n");
        }
    }
    else
    { // TODO show detailed status
        node->setStatus(OtauNode::StatusUnknownError);
        // TODO according spec. send default response with status SUCCESS
    }
}

/*! Sends a upgrade end response.
    \param node - the destination node
    \param upgradeTime - time in seconds to wait for switching to new image
    \return true on success false otherwise
 */
bool StdOtauPlugin::upgradeEndResponse(OtauNode *node, uint32_t upgradeTime)
{
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    DBG_Assert(node->address().hasExt());
    if (!node->address().hasExt())
    {
        return false;
    }

    if (node->upgradeEndReq.manufacturerCode == 0 &&
        node->upgradeEndReq.fileVersion == 0 &&
        node->upgradeEndReq.status != OTAU_SUCCESS)
    {
        DBG_Printf(DBG_INFO,"otau upgrade end response not send because status is not success but 0x%02X\n", node->upgradeEndReq.status);
        return false;
    }

    req.setProfileId(node->profileId);
    req.setDstEndpoint(node->endpoint);
    req.setClusterId(OTAU_CLUSTER_ID);
    req.dstAddress() = node->address();
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.setSrcEndpoint(m_srcEndpoint);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.setRadius(MAX_RADIUS);

    zclFrame.setSequenceNumber(node->reqSequenceNumber);
    zclFrame.setCommandId(OTAU_UPGRADE_END_RESPONSE_CMD_ID);

    zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    if (node->manufacturerId == VENDOR_DDEL && node->imageType() == IMG_TYPE_FLS_NB)
    {
        upgradeTime = 0xffff; // use DE Cluster for restart [1]
    }

    { // ZCL payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << node->upgradeEndReq.manufacturerCode;
        stream << node->upgradeEndReq.imageType;
        stream << node->upgradeEndReq.fileVersion;

        uint32_t currentTime = 0;

        stream << currentTime;
        stream << upgradeTime;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    bool ret = false;

    if (deCONZ::ApsController::instance()->apsdeDataRequest(req) == 0)
    {
        node->apsRequestId = req.id();
        node->zclCommandId = zclFrame.commandId();
        ret = true;
    }

    // [1] send delayed DE cluster WDT Reset command
    if (node->manufacturerId == VENDOR_DDEL && node->imageType() == IMG_TYPE_FLS_NB)
    {
        deCONZ::ApsDataRequest req2;

        req2.setProfileId(node->profileId);
        req2.setDstEndpoint(node->endpoint);
        req2.setClusterId(DE_CLUSTER_ID);
        req2.dstAddress() = node->address();
        req2.setDstAddressMode(deCONZ::ApsExtAddress);
        req2.setSrcEndpoint(m_srcEndpoint);
        req2.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
        req2.setRadius(MAX_RADIUS);
        req2.setSendDelay(3000); // send 3s after upgrade end response

        zclFrame.setSequenceNumber(node->reqSequenceNumber + 1);
        zclFrame.setCommandId(0x04); // write RAM

        zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                 deCONZ::ZclFCDirectionClientToServer |
                                 deCONZ::ZclFCDisableDefaultResponse);

        { // ZCL payload
            zclFrame.payload().clear();
            QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            quint16 offset = 0x8888;
            quint8 dataLength = 1;
            quint8 data0 = 1;
            quint8 dummy = 0;
            stream << offset;
            stream << dataLength;
            // payload must be 4 bytes
            stream << data0;
            stream << dummy;
            stream << dummy;
            stream << dummy;
        }

        { // ZCL frame
            QDataStream stream(&req2.asdu(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);
            zclFrame.writeToStream(stream);
        }

        if (deCONZ::ApsController::instance()->apsdeDataRequest(req2) == 0)
        {
        }
    }

    return ret;
}

bool StdOtauPlugin::defaultResponse(OtauNode *node, quint8 status)
{
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    DBG_Assert(node->address().hasExt());
    if (!node->address().hasExt())
    {
        return false;
    }

    req.setProfileId(node->profileId);
    req.setDstEndpoint(node->endpoint);
    req.setClusterId(OTAU_CLUSTER_ID);
    req.dstAddress() = node->address();
    req.setDstAddressMode(deCONZ::ApsExtAddress);

    req.setSrcEndpoint(m_srcEndpoint);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

    zclFrame.setSequenceNumber(node->reqSequenceNumber);
    req.setRadius(MAX_RADIUS);

    zclFrame.setCommandId(deCONZ::ZclDefaultResponseId);

    zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // ZCL payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << status;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (deCONZ::ApsController::instance()->apsdeDataRequest(req) == deCONZ::Success)
    {
        node->apsRequestId = req.id();
        node->zclCommandId = zclFrame.commandId();
        node->lastResponseTime.restart();
        return true;
    }

    return false;
}

/*! Queries this plugin which features are supported.
    \param feature - feature to be checked
    \return true if supported
 */
bool StdOtauPlugin::hasFeature(Features feature)
{
    switch (feature)
    {
    case WidgetFeature:
        return true;

    default:
        break;
    }

    return false;
}

/*! Creates a control widget for this plugin.
    \return the plugin widget
 */
QWidget *StdOtauPlugin::createWidget()
{
    if (!m_w)
    {
        m_w = new StdOtauWidget(0);

        connect(m_w, SIGNAL(unicastImageNotify(deCONZ::Address)),
                this, SLOT(unicastImageNotify(deCONZ::Address)));

        connect(m_w, SIGNAL(unicastUpgradeEndRequest(deCONZ::Address)),
                this, SLOT(unicastUpgradeEndRequest(deCONZ::Address)));

        connect(m_w, SIGNAL(broadcastImageNotify()),
                this, SLOT(broadcastImageNotify()));

        connect(m_w, SIGNAL(activatedNodeAtRow(int)),
                this, SLOT(activatedNodeAtRow(int)));

        m_w->setOtauModel(m_model);
    }

    return m_w;
}

/*! Creates a control dialog for this plugin.
    \return 0 - not implemented
 */
QDialog *StdOtauPlugin::createDialog()
{
    return 0;
}

/*! Returns the name of this plugin.
 */
const char *StdOtauPlugin::name()
{
    return "STD OTAU Plugin";
}

/*! Sets the plugin state.
    \param state - the new state
 */
void StdOtauPlugin::setState(StdOtauPlugin::State state)
{
    if (m_state != state)
    {
        m_state = state;
        emit stateChanged();
    }
}

/*! Checks if the node is a not yet known otau node.
    If the node is unknown it will be added to the model.
    \param node - the node to check
    \param endpoint - otau endpoint of the node
 */
void StdOtauPlugin::checkIfNewOtauNode(const deCONZ::Node *node, uint8_t endpoint)
{
    DBG_Assert(node != 0);
    if (!node)
    {
        return;
    }

    deCONZ::SimpleDescriptor sd;

    if (node->copySimpleDescriptor(endpoint, &sd) != 0)
    {
        return;
    }

    {
        QList<deCONZ::ZclCluster>::const_iterator i = sd.outClusters().begin();
        QList<deCONZ::ZclCluster>::const_iterator end = sd.outClusters().end();
        for (; i != end; ++i)
        {
            // okay has server image notify cluster
            if (i->id() == OTAU_CLUSTER_ID)
            {
                // create node if not exist
                bool create = true;
                OtauNode *otauNode = m_model->getNode(node->address(), create);
                Q_UNUSED(otauNode);

                if (otauNode && otauNode->profileId != sd.profileId())
                {

                    uint16_t profileId = 0;

                    if (sd.profileId() == ZLL_PROFILE_ID)
                    {
                        profileId = HA_PROFILE_ID;
                    }
                    else
                    {
                        profileId = sd.profileId();
                    }

                    if (profileId != otauNode->profileId)
                    {
                        DBG_Printf(DBG_INFO, "otau set node profileId to 0x%04X\n", profileId);
                        otauNode->profileId = profileId;
                    }
                }


                return;
            }
        }
    }
}

void StdOtauPlugin::activatedNodeAtRow(int row)
{
    OtauNode *node = m_model->getNodeAtRow(row);

    if (node)
    {
        m_w->displayNode(node);
    }
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(std_otau_plugin, StdOtauPlugin)
#endif
