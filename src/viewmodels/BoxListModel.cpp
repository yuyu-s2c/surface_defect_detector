#include "BoxListModel.h"

BoxListModel::BoxListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void BoxListModel::setRows(const QVector<Row>& rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

void BoxListModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

int BoxListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant BoxListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row& r = m_rows.at(index.row());
    switch (role) {
    case XRole:
        return r.x;
    case YRole:
        return r.y;
    case WidthRole:
        return r.width;
    case HeightRole:
        return r.height;
    case AreaRole:
        return r.area;
    default:
        return {};
    }
}

QHash<int, QByteArray> BoxListModel::roleNames() const
{
    return {
        {XRole, "x"},
        {YRole, "y"},
        {WidthRole, "width"},
        {HeightRole, "height"},
        {AreaRole, "area"},
    };
}
