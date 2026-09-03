#include "NgListModel.h"

NgListModel::NgListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void NgListModel::append(const Record& rec)
{
    const int row = m_rows.size();
    beginInsertRows({}, row, row);
    m_rows.push_back(rec);
    endInsertRows();
    emit countChanged();
}

void NgListModel::clear()
{
    if (m_rows.isEmpty() && m_selected < 0)
        return;
    beginResetModel();
    m_rows.clear();
    m_selected = -1;
    endResetModel();
    emit countChanged();
    emit selectedRowChanged();
}

const NgListModel::Record* NgListModel::recordAt(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return nullptr;
    return &m_rows.at(row);
}

void NgListModel::setSelectedRow(int row)
{
    if (row < -1 || row >= m_rows.size())
        row = -1;
    if (m_selected == row)
        return;
    m_selected = row;
    emit selectedRowChanged();
    if (!m_rows.isEmpty())
        emit dataChanged(index(0, 0), index(m_rows.size() - 1, 0), {SelectedRole});
}

int NgListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant NgListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Record& r = m_rows.at(index.row());
    switch (role) {
    case FileNameRole:
        return r.fileName;
    case DefectRole:
        return r.defectLabel;
    case ScoreRole:
        return QString::number(r.imageScore, 'f', 3);
    case LatencyRole:
        return int(r.latencyMs);
    case LateRole:
        return r.lateEject;
    case PathRole:
        return r.path;
    case CategoryRole:
        return r.category;
    case SelectedRole:
        return index.row() == m_selected;
    case Qt::DisplayRole:
        return r.fileName;
    default:
        return {};
    }
}

QHash<int, QByteArray> NgListModel::roleNames() const
{
    return {
        {FileNameRole, "fileName"},
        {DefectRole, "defectLabel"},
        {ScoreRole, "scoreText"},
        {LatencyRole, "latencyMs"},
        {LateRole, "lateEject"},
        {PathRole, "path"},
        {CategoryRole, "category"},
        {SelectedRole, "rowSelected"},
    };
}
