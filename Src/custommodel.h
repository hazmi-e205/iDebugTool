#ifndef CUSTOMMODEL_H
#define CUSTOMMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "logpacket.h"

class CustomModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit CustomModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setMaxData(qsizetype max_count);
    void addItem(const LogPacket& packet);
    void addItems(const QList<LogPacket>& packets);
    void clear();
    void removeOldData();
    LogPacket getLogPacket(qsizetype index);

private:
    QList<LogPacket> m_datalist;
    qsizetype m_maxdata;
};

#endif // CUSTOMMODEL_H
