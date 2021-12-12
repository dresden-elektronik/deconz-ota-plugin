#include <QFont>
#include "deconz.h"
#include "otau_file.h"
#include "otau_node.h"
#include "otau_model.h"
#include "std_otau_plugin.h"

/*! The constructor.
 */
OtauModel::OtauModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

OtauModel::~OtauModel()
{
    for (OtauNode *&n : m_nodes)
    {
        if (n)
        {
            delete n;
            n = nullptr;
        }
    }
    m_nodes.clear();
}

/*! Returns the model rowcount.
 */
int OtauModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_nodes.size();
}

/*! Returns the model columncount.
 */
int OtauModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return SectionCount;
}

/*! Returns the model headerdata for a column.
 */
QVariant OtauModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(orientation);

    if (role == Qt::DisplayRole && (orientation == Qt::Horizontal))
    {
        switch (section)
        {
        case SectionAddress:
            return tr("Address");

        case SectionManufacturer:
            return tr("Vendor");

        case SectionImageType:
            return tr("Image");

        case SectionSoftwareVersion:
            return tr("Version");

        case SectionProgress:
            return tr("Progress");

        case SectionDuration:
            return tr("Time");

//        case SectionStatus:
//            return tr("Status");

        default:
            return tr("Unknown");
        }
    }

    return QVariant();
}

/*! Returns the model data for a specific column.
 */
QVariant OtauModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (index.row() >= rowCount(QModelIndex()))
        {
            return QVariant();
        }

        QString str;
        OtauNode *node = m_nodes[index.row()];

        switch(index.column())
        {
        case SectionAddress:
            if (node->address().hasExt())
            {
                str = "0x" + QString("%1").arg(node->address().ext(), 16, 16, QLatin1Char('0')).toUpper();
            }
            else if (node->address().hasNwk())
            {
                str = "0x" + QString("%1").arg(node->address().nwk(), 4, 16, QLatin1Char('0')).toUpper();
            }
            break;

        case SectionManufacturer:
            str = "0x" + QString("%1").arg(node->manufacturerId, 4, 16, QLatin1Char('0'));
            break;


        case SectionImageType:
            str = "0x" + QString("%1").arg(node->imageType(), 4, 16, QLatin1Char('0'));
            break;

        case SectionSoftwareVersion:
            str = "0x" + QString("%1").arg(node->softwareVersion(), 8, 16, QLatin1Char('0'));
            break;

        case SectionProgress:
            if (node->status() == OtauNode::StatusWaitUpgradeEnd)
            {
                str = tr("Wait to finish");
            }
            else if (node->zclCommandId == OTAU_UPGRADE_END_RESPONSE_CMD_ID)
            {
                switch(node->upgradeEndReq.status)
                {
                case OTAU_SUCCESS:            str = tr("Done"); break;
                case OTAU_ABORT:              str = tr("Abort"); break;
                case OTAU_INVALID_IMAGE:      str = tr("Invalid image"); break;
                case OTAU_REQUIRE_MORE_IMAGE: str = tr("Require more image"); break;
                default:
                    str = tr("Unknown");
                    break;
                }
            }
            else if (node->zclCommandId == OTAU_QUERY_NEXT_IMAGE_RESPONSE_CMD_ID)
            {
                if (node->hasData())
                {
                    str = tr("Idle");
                }
                else
                {
                    str = tr("No file");
                }
            }
            else if (node->permitUpdate())
            {
                if (node->offset() > 0)
                {
                    if (node->offset() == node->file.totalImageSize)
                    {
                        str = tr("Done");
                    }
                    else
                    {
                        str = QString("%1%").arg((static_cast<double>(node->offset()) / static_cast<double>(node->file.totalImageSize)) * 100.0, 0, 'f', 2);
                    }
                }
                else
                {
                    str = tr("Queued");
                }
            }
            else if (node->hasData())
            {
                str = tr("Paused");
            }
            else
            {
                str = tr("No file");
            }
            break;

        case SectionDuration:
        {
            int min = (node->elapsedTime() / 1000) / 60;
            int sec = (node->elapsedTime() / 1000) % 60;
            str = QString("%1:%2").arg(min).arg(sec, 2, 10, QLatin1Char('0'));
        }
            break;

//        case SectionStatus:
//            str = node->statusString();
//            break;

        default:
            break;
        }

        return str;
    }
    else if (role == Qt::ToolTipRole)
    {
        if (index.row() >= rowCount(QModelIndex()))
        {
            return QVariant();
        }

        OtauNode *node = m_nodes[index.row()];

        switch (index.column())
        {
        case SectionSoftwareVersion:
            // dresden electronic spezific version format
            if (node->softwareVersion() != 0)
            {
                if ((node->address().ext() & 0x00212EFFFF000000ULL) != 0)
                {
                    return QString("%1.%2 build %3")
                            .arg((node->softwareVersion() & 0xF0000000U) >> 28)
                            .arg((node->softwareVersion() & 0x0FF00000U) >> 20)
                            .arg((node->softwareVersion() & 0x000FFFFFU));
                }
            }
            break;

        default:
            break;
        }
    }
    else if (role == Qt::FontRole)
    {
        switch (index.column())
        {
        case SectionAddress:
        case SectionManufacturer:
        case SectionImageType:
        case SectionSoftwareVersion:
        {
            QFont font("Monospace");
            font.setStyleHint(QFont::TypeWriter);
            return font;
        }

        default:
            break;
        }
    }

    return QVariant();
}

/*! Returns a OtauNode.
    \param addr - the nodes address which must contain nwk and ext address
    \param create - true if a OtauNode shall be created if it does not exist yet
    \return pointer to a OtauNode or 0 if not found
 */
OtauNode *OtauModel::getNode(const deCONZ::Address &addr, bool create)
{
    if (!addr.hasExt() && !addr.hasNwk())
    {
        return nullptr;
    }

    for (OtauNode *i : m_nodes)
    {
        if (addr.hasExt() && i->address().hasExt())
        {
            if (i->address().ext() == addr.ext())
            {
                if (i->address().nwk() != addr.nwk())
                {
                    // update nwk address
                }
                return i;
            }
        }

        if (addr.hasNwk() && i->address().hasNwk())
        {
            if (i->address().nwk() == addr.nwk())
            {
                return i;
            }
        }
    }

    if (create && addr.hasExt() && addr.hasNwk())
    {
        // not found create new
        uint row = m_nodes.size();

        beginInsertRows(QModelIndex(), row, row);
        OtauNode *node = new OtauNode(addr);
        node->row = row;
        node->model = this;
        m_nodes.push_back(node);
        endInsertRows();
        DBG_Printf(DBG_OTA, "OTAU: node added %s\n", qPrintable(addr.toStringExt()));
        return node;
    }

    return 0;
}

/*! Returns a OtauNode for a given row.
 */
OtauNode *OtauModel::getNodeAtRow(uint row)
{
    if (m_nodes.size() > row)
    {
        return m_nodes[row];
    }

    return 0;
}

/*! Notify model/view that the data for a given node has changed.
 */
void OtauModel::nodeDataUpdate(OtauNode *node)
{
    if (node && (node->row < m_nodes.size()))
    {
        emit dataChanged(index(node->row, 0), index(node->row, SectionCount - 1), {Qt::DisplayRole});
    }
}

/*! Returns the internal vector of nodes.
 */
std::vector<OtauNode *> &OtauModel::nodes()
{
    return m_nodes;
}
