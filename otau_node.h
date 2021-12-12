#ifndef OTAU_NODE_H
#define OTAU_NODE_H

#include <array>
#include <QTime>
#include <QElapsedTimer>
#include "deconz/types.h"
#include "deconz/aps.h"
#include "deconz/timeref.h"

#define NODE_TIMEOUT        10000
#define MAX_ACTIVE_BLOCK_REQUESTS 9

struct OtauFile;
class OtauModel;

struct ImageNotifyReq
{
    // adressing
    deCONZ::ApsAddressMode addrMode;
    deCONZ::Address addr;
    uint8_t dstEndpoint;
    uint8_t radius;
};

struct ImageBlockReq
{
    uint8_t fieldControl;
    uint16_t manufacturerCode;
    uint16_t imageType;
    uint32_t fileVersion;
    uint32_t offset;
    uint8_t maxDataSize;
    // image page request extra fields
    uint16_t pageBytesDone;
    uint16_t pageSize;
    uint16_t responseSpacing;
};

struct ImageBlockReqTrack
{
    deCONZ::SteadyTimeRef sendTime;
    uint8_t apsRequestId;
    uint8_t retry;
};

struct UpgradeEndReq
{
    UpgradeEndReq() :
        status(0), manufacturerCode(0), imageType(0), fileVersion(0)
    {}
    uint8_t status;
    uint16_t manufacturerCode;
    uint16_t imageType;
    uint32_t fileVersion;
};

/*! \class OtauNode

    Represents a otau node and its state/data.
 */
struct OtauNode
{
public:
    enum NodeState
    {
        NodeIdle,
        NodeBusy,
        NodeWaitPageSpacing,
        NodeWaitNextRequest,
        NodeWaitConfirm,
        NodeError,
        NodeAbort
    };

    enum Status
    {
        StatusSuccess            = 0x00,
        StatusInvalidParameter   = 0x01,
        StatusWrongOffset        = 0x02,
        StatusUnknownError       = 0x03,
        StatusAbort              = 0x04,
        StatusWrongImageType     = 0x05,
        StatusWrongManufacturer  = 0x06,
        StatusWrongPlatform      = 0x07,
        StatusTimeout            = 0x08,
        StatusIgnored            = 0x09,
        StatusCrcError           = 0x0A,
        StatusWaitUpgradeEnd     = 0x0B
    };

    OtauNode(const deCONZ::Address &addr);

    NodeState state() { return m_state; }
    void setState(NodeState state);
    const deCONZ::Address &address() { return m_addr; }
    void setAddress(const deCONZ::Address &addr);
    uint32_t softwareVersion() { return m_swVersion; }
    void setSoftwareVersion(uint32_t version);
    uint32_t hardwareVersion() { return m_hwVersion; }
    void setHardwareVersion(uint16_t version);
    uint32_t offset() { return m_offset; }
    void setOffset(uint32_t offset);
    uint16_t imageType() { return m_imageType; }
    void setImageType(uint16_t type);
    void refreshTimeout();
    bool permitUpdate() { return m_permitUpdate; }
    void setPermitUpdate(bool permit) { m_permitUpdate = permit; }
    bool hasData() { return m_hasData; }
    void setHasData(bool hasData) { m_hasData = hasData; }
    void restartElapsedTimer();
    void notifyElapsedTimer();
    int elapsedTime() { return m_elapsedTime; }
    Status status() const { return m_status; }
    void setStatus(Status status) { m_status = status; }
    QString statusString() const;
    void setLastZclCommand(uint8_t commandId);
    uint8_t lastZclCmd() const;
    const QTime &lastQueryTime() const { return m_lastQueryTime; }

    // service for model
    uint row;
    OtauModel *model;
    bool rxOnWhenIdle;

    // TODO: getter and setter
    uint16_t apsRequestId;
    uint8_t zclCommandId; // last send ZCL command id
    uint8_t endpoint;
    uint8_t endpointNotify;
    uint8_t reqSequenceNumber;
    uint16_t profileId;
    uint16_t manufacturerId;
    uint8_t maxDataSize;
    uint32_t imageSize;
    uint8_t *imageData;
    uint32_t timeout; // seconds
    QElapsedTimer lastResponseTime;
    QElapsedTimer lastActivity;

    OtauFile file;
    QByteArray rawFile;
    ImageBlockReq imgPageReq;
    ImageBlockReq imgBlockReq;
    UpgradeEndReq upgradeEndReq;
    int imgPageRequestRetry;
    int imgBlockResponseRetry;

    std::array<ImageBlockReqTrack, MAX_ACTIVE_BLOCK_REQUESTS> imgBlockTrack{};

private:
    deCONZ::Address m_addr;
    NodeState m_state;
    uint8_t m_lastZclCmd;
    uint32_t m_swVersion;
    uint16_t m_hwVersion;
    uint32_t m_offset;
    uint16_t m_imageType;
    bool m_hasData;
    bool m_permitUpdate;
    QElapsedTimer m_time;
    QTime m_lastQueryTime;
    int m_elapsedTime;
    Status m_status;
};

#endif // OTAU_NODE_H
