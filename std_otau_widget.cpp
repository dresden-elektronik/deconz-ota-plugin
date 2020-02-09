#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QTimer>
#include <stdint.h>
#include "deconz/crc8.h"
#include "std_otau_widget.h"
#include "std_otau_plugin.h"
#include "otau_file_loader.h"
#include "otau_model.h"
#include "otau_node.h"
#include "ui_std_otau_widget.h"

#define UNLIMITED_WATING_TIME 0xfffffffful

StdOtauWidget::StdOtauWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::StdOtauWidget)
{
    ui->setupUi(this);

    // OTAU update tab
    m_ouNode = 0;

    connect(ui->ou_queryButton, SIGNAL(clicked()),
            this, SLOT(queryClicked()));

    connect(ui->ou_abortButton, SIGNAL(clicked()),
            this, SLOT(abortClicked()));

    connect(ui->ou_updateButton, SIGNAL(clicked()),
            this, SLOT(updateClicked()));

    connect(ui->ou_fileSelectButton, SIGNAL(clicked()),
            this, SLOT(fileSelectClicked()));

    connect(ui->tableView, SIGNAL(clicked(QModelIndex)),
            this, SLOT(otauTableActivated(QModelIndex)));

    // OTAU file tab
    connect(ui->saveButton, SIGNAL(clicked()),
            this, SLOT(saveClicked()));

    connect(ui->saveAsButton, SIGNAL(clicked()),
            this, SLOT(saveAsClicked()));


    connect(ui->openButton, SIGNAL(clicked()),
            this, SLOT(openClicked()));

    // adjust columns
    ui->tableView->resizeColumnToContents(OtauModel::SectionAddress);
    ui->tableView->resizeColumnToContents(OtauModel::SectionSoftwareVersion);
    ui->tableView->resizeColumnToContents(OtauModel::SectionImageType);
}

StdOtauWidget::~StdOtauWidget()
{
    delete ui;
}

void StdOtauWidget::setOtauModel(OtauModel *model)
{
    ui->tableView->setModel(model);
}

uint StdOtauWidget::restartTime()
{
    if (ui->restartAfterUpgradeCheckbox->isChecked())
    {
        return ui->restartAfterUpgradeSpinBox->value();
    }
    return UNLIMITED_WATING_TIME;
}

void StdOtauWidget::queryClicked()
{
    if (m_ouNode)
    {
        emit unicastImageNotify(m_ouNode->address());
    }
}

void StdOtauWidget::abortClicked()
{
    if (m_ouNode)
    {
        m_ouNode->setPermitUpdate(false);
        m_ouNode->setState(OtauNode::NodeAbort);
    }
}

void StdOtauWidget::updateClicked()
{
    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();
    if (!apsCtrl)
    {
        return;
    }

    // if otau is not activated yet do it here since thats what the user expects
    if (apsCtrl->getParameter(deCONZ::ParamOtauActive) == 0)
    {
        if (!apsCtrl->setParameter(deCONZ::ParamOtauActive, 1))
        {
            DBG_Printf(DBG_OTA, "failed to enable otau server\n");
        }
    }

    if (m_ouNode)
    {
        m_ouNode->setState(OtauNode::NodeIdle);
        if (m_ouNode->hasData())
        {
            m_ouNode->setPermitUpdate(true);
            emit unicastImageNotify(m_ouNode->address());
        }
    }
}

void StdOtauWidget::fileSelectClicked()
{
    if (m_ouNode)
    {
        QString dirpath;

        if (!m_path.isEmpty())
        {
            QFileInfo fi(m_path);
            dirpath = fi.dir().absolutePath();
        }

        if (dirpath.isEmpty())
        {
            QString defaultImgPath = deCONZ::getStorageLocation(deCONZ::HomeLocation) + "/otau";
            dirpath = deCONZ::appArgumentString("--otau-img-path", defaultImgPath);
        }

        QString path = QFileDialog::getOpenFileName(this,
                                                    "Select a firmware file",
                                                    dirpath,
                                                    "Firmware (*.zigbee *.ota.signed *.ota)");
        if (path.isEmpty())
        {
            clearSettingsBox();
        }
        else
        {
            OtauFileLoader ld;

            if (ld.readFile(path, m_ouNode->file))
            {
                m_ouNode->setHasData(true);
                m_ouNode->lastActivity.restart();
                updateSettingsBox();
            }
            else
            {
                clearSettingsBox();
            }
        }
    }
}

bool StdOtauWidget::acksEnabled() const
{
    return ui->useAcksCheckBox->isChecked();
}

bool StdOtauWidget::pageRequestEnabled() const
{
    return ui->usePageRequestCheckBox->isChecked();
}

int StdOtauWidget::packetSpacingMs() const
{
    return ui->spacingSpinBox->value();
}

void StdOtauWidget::setPacketSpacingMs(int spacing)
{
    ui->spacingSpinBox->setValue(spacing);
}

void StdOtauWidget::stateChanged(int state)
{
    if (state == StdOtauPlugin::StateDisabled)
    {
        ui->labelOtauState->setText(tr("OTAU disabled"));
    }
    else if (state == StdOtauPlugin::StateDisabled)
    {
        ui->labelOtauState->setText(tr("OTAU wait sensors idle"));
    }
    else
    {
        ui->labelOtauState->setText(tr("OTAU enabled"));
    }
}

void StdOtauWidget::clearSettingsBox()
{
    ui->ou_fileEdit->setText(QString());
    ui->ou_fileVersionEdit->setText("0x00000000");
    ui->ou_fileVersionEdit->setToolTip(QString());
    ui->ou_imageTypeEdit->setText("0x0000");
    ui->ou_SizeEdit->setText("0x00000000");
}

void StdOtauWidget::updateSettingsBox()
{
    if (m_ouNode)
    {
        if (m_ouNode->hasData())
        {
            const OtauFile &of = m_ouNode->file;

            ui->ou_fileEdit->setText(of.path);

            QString str = QString("0x%1").arg(of.fileVersion, 8, 16, QLatin1Char('0')).toUpper();
            ui->ou_fileVersionEdit->setText(str);

            QString version;
            if (of.fileVersion != 0)
            {
                version = QString("%1.%2 build %3").arg((of.fileVersion & 0xF0000000U) >> 28).arg((of.fileVersion & 0x0FF00000U) >> 20).arg((of.fileVersion & 0x000FFFFFU));
            }
            ui->ou_fileVersionEdit->setToolTip(version);

            str = QString("0x%1").arg(of.imageType, 4, 16, QLatin1Char('0')).toUpper();
            ui->ou_imageTypeEdit->setText(str);

            str = QString("0x%1").arg(of.manufacturerCode, 4, 16, QLatin1Char('0')).toUpper();
            ui->ou_manufacturerEdit->setText(str);

            str = QString("0x%1 (%2 kB)").arg(of.totalImageSize, 8, 16, QLatin1Char('0')).arg(of.totalImageSize / 1014).toUpper();
            ui->ou_SizeEdit->setText(str);
        }
        else
        {
            clearSettingsBox();
        }
    }
}

void StdOtauWidget::otauTableActivated(const QModelIndex &index)
{
    if (index.isValid())
    {
        emit activatedNodeAtRow(index.row());
    }
}

void StdOtauWidget::saveClicked()
{
    if (m_path.endsWith(".bin"))
    {
        m_path.replace(".bin", ".zigbee");
        ui->fileNameLabel->setText(m_path);
    }
    else if (m_path.endsWith(".bin.GCF"))
    {
        m_path.replace(".bin.GCF", ".zigbee");
        ui->fileNameLabel->setText(m_path);
    }
    else if (m_path.endsWith(".GCF"))
    {
        m_path.replace(".GCF", ".zigbee");
        ui->fileNameLabel->setText(m_path);
    }

    m_editOf.fileVersion = ui->of_FileVersionEdit->text().toUInt(0, 16);
    m_editOf.headerVersion = ui->of_headerVersionEdit->text().toUShort(0, 16);
    m_editOf.imageType = ui->of_imageTypeEdit->text().toUShort(0, 16);
    m_editOf.manufacturerCode = ui->of_manufacturerEdit->text().toUShort(0, 16);
    m_editOf.zigBeeStackVersion = ui->of_zigbeeStackVersionEdit->text().toUShort(0, 16);

    // TODO: description


    //  preserve sub element "upgrade image"
    OtauFile::SubElement subImage;
    {
        std::list<OtauFile::SubElement>::iterator it = m_editOf.subElements.begin();
        std::list<OtauFile::SubElement>::iterator end = m_editOf.subElements.end();

        for (;it != end; ++it)
        {
            if (it->tag == TAG_UPGRADE_IMAGE)
            {
                subImage = *it;
            }
        }
    }
    // clean all elements
    m_editOf.subElements.clear();
    // restore upgrade image as first sub element
    m_editOf.subElements.push_back(subImage);

    OtauFileLoader ld;
    if (!ld.saveFile(m_path, m_editOf))
    {

    }
}

void StdOtauWidget::saveAsClicked()
{
}

void StdOtauWidget::openClicked()
{
    QString path;

    if (!m_path.isEmpty())
    {
        QFileInfo fi(m_path);
        path = fi.dir().absolutePath();
    }

    if (path.isEmpty())
    {
        QString defaultImgPath = deCONZ::getStorageLocation(deCONZ::HomeLocation) + "/otau";
        path = deCONZ::appArgumentString("--otau-img-path", defaultImgPath);
    }

    m_path = QFileDialog::getOpenFileName(this,
                                     tr("Select a firmware file"),
                                     path,
                                     "Firmware (*.GCF *.bin *.zigbee *.ota.signed *.ota)");

    if (!m_path.isEmpty())
    {

        OtauFileLoader ld;

        if (ld.readFile(m_path, m_editOf))
        {
            ui->fileNameLabel->setText(m_path);
            updateEditor();
        }
        else
        {
            ui->fileNameLabel->setText(tr("Invalid file"));
        }
    }
}

void StdOtauWidget::displayNode(OtauNode *node)
{
    m_ouNode = node;

    if (node)
    {
        updateSettingsBox();

        if (node->lastQueryTime().isValid())
        {
            ui->lastQueryLabel->setText(node->lastQueryTime().toString("hh:mm:ss"));
        }
        else
        {
            ui->lastQueryLabel->setText(tr("None"));
        }
    }
    else
    {
        ui->lastQueryLabel->setText(tr("None"));
        clearSettingsBox();
    }


}

void StdOtauWidget::updateEditor()
{
    QString str;

    str = QString("0x%1").arg(m_editOf.fileVersion, 8, 16, QLatin1Char('0')).toUpper();
    ui->of_FileVersionEdit->setText(str);

    str = QString("0x%1").arg(m_editOf.headerVersion, 4, 16, QLatin1Char('0')).toUpper();
    ui->of_headerVersionEdit->setText(str);

    str = QString("0x%1").arg(m_editOf.imageType, 4, 16, QLatin1Char('0')).toUpper();
    ui->of_imageTypeEdit->setText(str);

    str = QString("0x%1").arg(m_editOf.manufacturerCode, 4, 16, QLatin1Char('0')).toUpper();
    ui->of_manufacturerEdit->setText(str);

    str = QString("0x%1").arg(m_editOf.zigBeeStackVersion, 4, 16, QLatin1Char('0')).toUpper();
    ui->of_zigbeeStackVersionEdit->setText(str);

    QString descr;
    for (size_t i = 0; i < sizeof(m_editOf.headerString); i++)
    {
        if (isprint(m_editOf.headerString[i]))
        {
            descr.append((char)m_editOf.headerString[i]);
        }
        else
        {
            descr.append(' ');
        }
    }
    ui->of_descriptionEdit->setPlainText(descr);

    str = QString("0x%1").arg(m_editOf.minHardwareVersion, 4, 16, QLatin1Char('0')).toUpper();
    ui->of_minHwVersionEdit->setText(str);

    str = QString("0x%1").arg(m_editOf.maxHardwareVersion, 4, 16, QLatin1Char('0')).toUpper();
    ui->of_maxHwVersionEdit->setText(str);

    // standard 0
    str = QString("0x%1").arg(0, 8, 16, QLatin1Char('0')).toUpper();
    ui->of_firmwareSizeEdit->setText(str);

    {
        std::list<OtauFile::SubElement>::iterator it = m_editOf.subElements.begin();
        std::list<OtauFile::SubElement>::iterator end = m_editOf.subElements.end();

        for (;it != end; ++it)
        {
            if (it->tag == TAG_UPGRADE_IMAGE)
            {
                str = QString("0x%1 (%2 kB)").arg(it->length, 8, 16, QLatin1Char('0')).arg(it->length / 1024).toUpper();
                ui->of_firmwareSizeEdit->setText(str);
            }
        }
    }
}
