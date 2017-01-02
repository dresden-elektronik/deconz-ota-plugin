#ifndef STD_OTAU_WIDGET_H
#define STD_OTAU_WIDGET_H

#include <QWidget>
#include "deconz/types.h"
#include "deconz/aps.h"
#include "otau_file.h"

namespace Ui {
class StdOtauWidget;
}

class StdOtauPlugin;
struct OtauFile;
class OtauModel;
struct OtauNode;
class QModelIndex;

class StdOtauWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit StdOtauWidget(QWidget *parent);
    ~StdOtauWidget();
    void setOtauModel(OtauModel *model);
    uint restartTime();

public Q_SLOTS:
    void stateChanged();
    void clearSettingsBox();
    void updateSettingsBox();
    void otauTableActivated(const QModelIndex &index);

    // OTAU upgrade
    void finishClicked();
    void findClicked();
    void queryClicked();
    void abortClicked();
    void updateClicked();
    void fileSelectClicked();

    bool acksEnabled() const;
    bool pageRequestEnabled() const;
    int packetSpacingMs() const;
    void setPacketSpacingMs(int spacing);

    // OTAU file tab
    void saveClicked();
    void saveAsClicked();
    void openClicked();
    void displayNode(OtauNode *node);

Q_SIGNALS:
    void broadcastImageNotify();
    void activatedNodeAtRow(int);
    void unicastImageNotify(deCONZ::Address);
    void unicastUpgradeEndRequest(deCONZ::Address);

private:
    void updateEditor();

    Ui::StdOtauWidget *ui;
    QString m_path;
    OtauFile m_editOf;
    OtauNode *m_ouNode;
};

#endif // STD_OTAU_WIDGET_H
