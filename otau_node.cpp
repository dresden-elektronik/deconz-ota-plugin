#include <stdint.h>
#include "deconz/types.h"
#include "otau_file.h"
#include "otau_node.h"
#include "otau_model.h"

#define OTAU_QUERY_NEXT_IMAGE_REQUEST_CMD_ID   0x01

/*! The constructor.
 */
OtauNode::OtauNode(const deCONZ::Address &addr)
    : m_addr(addr)
{
    m_state = NodeIdle;
    m_lastZclCmd = 0;
    m_swVersion = 0;
    m_hwVersion = 0;
    m_offset = 0;
    m_imageType = 0;
    m_hasData = false;
    m_permitUpdate = false;
    m_elapsedTime = 0;
    m_time.start();
    m_status = StatusSuccess;
    apsRequestId = 0xff + 1; // invalid > 8-bit
    profileId = HA_PROFILE_ID;
    manufacturerId = 0;
    endpoint = 0xFF; // for unicast if endpoint not known
    endpointNotify = 0;
    rxOnWhenIdle = true;
}

/*! Sets the nodes state.
    \param state - the new otau node state
 */
void OtauNode::setState(OtauNode::NodeState state)
{
    if (state != m_state)
    {
        m_state = state;
        model->nodeDataUpdate(this);
    }
}

/*! Sets the address of the node.
    \param addr - the nodes address
 */
void OtauNode::setAddress(const deCONZ::Address &addr)
{
    if (m_addr != addr)
    {
        m_addr = addr;
        model->nodeDataUpdate(this);
    }
}

/*! Sets the node software version.
    \param version - the 32-bit software/firmware version
 */
void OtauNode::setSoftwareVersion(uint32_t version)
{
    if (m_swVersion != version)
    {
        m_swVersion = version;
        model->nodeDataUpdate(this);
    }
}

/*! Sets the node hardware version.
    \param version - the 16-bit hardware version
 */
void OtauNode::setHardwareVersion(uint16_t version)
{
    if (m_hwVersion != version)
    {
        m_hwVersion = version;
        model->nodeDataUpdate(this);
    }
}

/*! Sets the current otau process file offset.
    \param offset - the current offset for file transfer
 */
void OtauNode::setOffset(uint32_t offset)
{
    if (m_offset != offset)
    {
        m_offset = offset;
        model->nodeDataUpdate(this);
    }
}

/*! Sets the otau file image type.
    \param type - the 16-bit otau file image type
 */
void OtauNode::setImageType(uint16_t type)
{
    if (m_imageType != type)
    {
        m_imageType = type;
        model->nodeDataUpdate(this);
    }
}

/*! Sets the command id of the last received ZCL command.
    \param commandId - the ZCL command id
 */
void OtauNode::setLastZclCommand(uint8_t commandId)
{
    m_lastZclCmd = commandId;

    if (commandId == OTAU_QUERY_NEXT_IMAGE_REQUEST_CMD_ID)
    {
        m_lastQueryTime = QTime::currentTime();
    }
}

/*! Returns the last received ZCL command id.
 */
uint8_t OtauNode::lastZclCmd() const
{
    return m_lastZclCmd;
}

/*! Refreshs/resets the otau process timeout.
 */
void OtauNode::refreshTimeout()
{
    timeout = NODE_TIMEOUT;
}

/*! Restarts the timer to measure the elapsed process time.
 */
void OtauNode::restartElapsedTimer()
{
    m_elapsedTime = 0;
    m_time.restart();
    model->nodeDataUpdate(this);
}

/*! Notify views to update elapsed time value.
 */
void OtauNode::notifyElapsedTimer()
{
    if (m_elapsedTime != m_time.elapsed())
    {
        m_elapsedTime = m_time.elapsed();
        model->nodeDataUpdate(this);
    }
}

/*! Returns string for nodes current status.
 */
QString OtauNode::statusString() const
{
    switch (status())
    {
    case StatusSuccess: return "Ok";
    case StatusInvalidParameter: return "InvalidParameter";
    case StatusWrongOffset: return "WrongOffset";
    case StatusUnknownError: return "UnknownError";
    case StatusAbort: return "Abort";
    case StatusWrongImageType: return "WrongImageType";
    case StatusWrongManufacturer: return "WrongManufacturer";
    case StatusWrongPlatform: return "WrongPlatform";
    case StatusTimeout: return "Timeout";
    case StatusIgnored: return "Ignored";
    case StatusCrcError: return "CrCError";
    case StatusWaitUpgradeEnd: return "WaitUpgradeEnd";
    default:
        break;
    }

    return "Unknown";
}
