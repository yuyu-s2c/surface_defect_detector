#include "DoPulseModel.h"

DoPulseModel::DoPulseModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void DoPulseModel::append(const Record& rec)
{
    const int row = m_rows.size();
    beginInsertRows({}, row, row);
    m_rows.push_back(rec);
    endInsertRows();
    emit countChanged();
}

void DoPulseModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

int DoPulseModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant DoPulseModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Record& r = m_rows.at(index.row());
    switch (role) {
    case SeqRole:
        return r.seq;
    case PointRole:
        return r.point;
    case ActionRole:
        return r.action;
    case PulseMsRole:
        return r.pulseMs;
    case FileNameRole:
        return r.fileName;
    case DefectRole:
        return r.defectLabel;
    case LatencyRole:
        return int(r.latencyMs);
    case LateRole:
        return r.lateEject;
    case Qt::DisplayRole:
        return r.point + QLatin1Char(' ') + r.action;
    default:
        return {};
    }
}

QHash<int, QByteArray> DoPulseModel::roleNames() const
{
    return {
        {SeqRole, "seq"},
        {PointRole, "point"},
        {ActionRole, "action"},
        {PulseMsRole, "pulseMs"},
        {FileNameRole, "fileName"},
        {DefectRole, "defectLabel"},
        {LatencyRole, "latencyMs"},
        {LateRole, "lateEject"},
    };
}
