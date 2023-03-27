#include "custommodel.h"

CustomModel::CustomModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int CustomModel::rowCount(const QModelIndex &parent) const
{
    return m_datalist.count();
}

int CustomModel::columnCount(const QModelIndex &parent) const
{
    return 5;
}

QVariant CustomModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole)
    {
        LogPacket packet = m_datalist.at(index.row());
        switch (index.column())
        {
        case 0:
            return packet.getDateTime();
        case 1:
            return packet.getDeviceName();
        case 2:
            return packet.getProcessID();
        case 3:
            return packet.getLogType();
        case 4:
            return packet.getLogMessage();
        }
    }
    return QVariant();
}

QVariant CustomModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
        {
            switch (section) {
            case 0:
                return QString("DateTime");
            case 1:
                return QString("DeviceName");
            case 2:
                return QString("ProcessID");
            case 3:
                return QString("Type");
            case 4:
                return QString("Messages");
            }
        }
        else
        {
            return section + 1;
        }
    }
    return QVariant();
}

void CustomModel::setMaxData(quint64 max_count)
{
    m_maxdata = max_count;
    removeOldData();
}

void CustomModel::addItem(const LogPacket &packet)
{
    int idx = m_datalist.count();
    beginInsertRows(QModelIndex(), idx, idx);
    m_datalist.append(packet);
    endInsertRows();
    removeOldData();
}

void CustomModel::clear()
{
    beginResetModel();
    m_datalist.clear();
    endResetModel();
}

void CustomModel::removeOldData()
{
    if (m_datalist.count() > m_maxdata)
    {
        qsizetype deleteCount = m_datalist.count() - m_maxdata;
        beginRemoveRows(QModelIndex(), 0, deleteCount - 1);
        m_datalist.remove(0, deleteCount);
        endRemoveRows();
    }
}
