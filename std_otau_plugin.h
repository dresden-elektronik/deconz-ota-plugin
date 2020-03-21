#ifndef STD_OTAU_PLUGIN_H
#define STD_OTAU_PLUGIN_H

#include <QObject>
#include <QElapsedTimer>
#include <list>
#include "deconz.h"
#include "otau_file.h"

#define ONOFF_CLUSTER_ID 0x0006
#define LEVEL_CLUSTER_ID 0x0008
#define OTAU_CLUSTER_ID  0x0019
#define DE_CLUSTER_ID    0xFC00

#define OTAU_IMAGE_NOTIFY_CMD_ID               0x00
#define OTAU_QUERY_NEXT_IMAGE_REQUEST_CMD_ID   0x01
#define OTAU_QUERY_NEXT_IMAGE_RESPONSE_CMD_ID  0x02
#define OTAU_IMAGE_BLOCK_REQUEST_CMD_ID        0x03
#define OTAU_IMAGE_PAGE_REQUEST_CMD_ID         0x04
#define OTAU_IMAGE_BLOCK_RESPONSE_CMD_ID       0x05
#define OTAU_UPGRADE_END_REQUEST_CMD_ID        0x06
#define OTAU_UPGRADE_END_RESPONSE_CMD_ID       0x07

/*! Otau ZCL status codes. */
typedef enum
{
    OTAU_SUCCESS                = 0x00,
    OTAU_ABORT                  = 0x95,
    OTAU_NOT_AUTHORIZED         = 0x7E,
    OTAU_INVALID_IMAGE          = 0x96,
    OTAU_WAIT_FOR_DATA          = 0x97,
    OTAU_NO_IMAGE_AVAILABLE     = 0x98,
    OTAU_MALFORMED_COMMAND      = 0x80,
    OTAU_UNSUP_CLUSTER_COMMAND  = 0x81,
    OTAU_REQUIRE_MORE_IMAGE     = 0x99
} OtauStatus_t;

class QFileSystemWatcher;
class StdOtauWidget;
struct OtauNode;
struct ImageNotifyReq;
class OtauModel;

class StdOtauPlugin : public QObject,
                     public deCONZ::NodeInterface
{
    Q_OBJECT
    Q_INTERFACES(deCONZ::NodeInterface)
#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID "org.dresden-elektronik.StdOtauPlugin")
#endif
public:
    enum State
    {
        StateEnabled,
        StateDisabled,
        StateBusySensors
    };

    explicit StdOtauPlugin(QObject *parent = 0);
    const char *name();
    bool hasFeature(Features feature);
    QWidget *createWidget();
    QDialog *createDialog();
    State state() const { return m_state; }

public Q_SLOTS:
    void apsdeDataIndication(const deCONZ::ApsDataIndication &ind);
    void apsdeDataConfirm(const deCONZ::ApsDataConfirm &conf);
    bool imageNotify(ImageNotifyReq *notf);
    void activatedNodeAtRow(int row);
    bool broadcastImageNotify();
    bool unicastImageNotify(const deCONZ::Address &addr);
    void unicastUpgradeEndRequest(const deCONZ::Address &addr);
    void matchDescriptorRequest(const deCONZ::ApsDataIndication &ind);
    void queryNextImageRequest(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);
    bool queryNextImageResponse(OtauNode *node);
    void imageBlockRequest(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);
    bool imageBlockResponse(OtauNode *node);
    void imagePageRequest(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);
    bool imagePageResponse(OtauNode *node);
    void upgradeEndRequest(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);
    bool upgradeEndResponse(OtauNode *node, uint32_t upgradeTime);
    bool defaultResponse(OtauNode *node, quint8 commandId, quint8 status);
    void nodeEvent(const deCONZ::NodeEvent &event);
    void nodeSelected(const deCONZ::Node *node);
    bool checkForUpdateImageImage(OtauNode *node, const QString &path);
    void invalidateUpdateEndRequest(OtauNode *node);
    void delayedImageNotify();
    void imagePageTimerFired();
    void cleanupTimerFired();
    void activityTimerFired();
    void markOtauActivity(const deCONZ::Address &address);
    bool otauIsActive() { return m_activityCounter > 0; }
    void checkFileLinks();

Q_SIGNALS:
    void stateChanged(int state);

private:
    void setState(State state);
    void checkIfNewOtauNode(const deCONZ::Node *node, uint8_t endpoint);
    deCONZ::Address m_delayedImageNotifyAddr;
    QString m_imgPath;
    OtauModel *m_model;
    State m_state;
    quint8 m_srcEndpoint;
    int m_sensorSlowdown;
    bool m_hasflsNb;
    StdOtauWidget *m_w;
    quint8 m_zclSeq;
    quint8 m_maxAsduDataSize;
    QTimer *m_imagePageTimer;
    QTimer *m_cleanupTimer;
    QTimer *m_activityTimer;
    int m_activityCounter;
    deCONZ::Address m_activityAddress;
    QElapsedTimer m_sensorActivity;
    int m_fastPageSpaceing;
    int m_slowPageSpaceing;
    bool m_sensorDontStart;
    bool m_sensorBusyRestart;
};

#endif // STD_OTAU_PLUGIN_H
