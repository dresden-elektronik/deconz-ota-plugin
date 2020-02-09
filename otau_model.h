#ifndef OTAU_MODEL_H
#define OTAU_MODEL_H

#include <vector>
#include <QAbstractTableModel>
#include "deconz/types.h"
#include "deconz/aps.h"

struct OtauNode;

/*! \class OtauModel

    Model which holds and represents all otau activity of nodes.
 */
class OtauModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Section
    {
        SectionAddress = 0,
        SectionSoftwareVersion,
        SectionImageType,
        SectionProgress,
        SectionDuration,
//        SectionStatus,

        SectionCount
    };

    explicit OtauModel(QObject *parent = nullptr);
    ~OtauModel();
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QVariant data(const QModelIndex &index, int role) const;

    OtauNode *getNode(const deCONZ::Address &addr, bool create = false);
    OtauNode *getNodeAtRow(uint row);
    void nodeDataUpdate(OtauNode *node);
    std::vector<OtauNode *> &nodes();
signals:
    
public slots:
private:
    std::vector<OtauNode*> m_nodes;
};

#endif // OTAU_MODEL_H
