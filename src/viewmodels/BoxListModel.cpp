#include "BoxListModel.h"

BoxListModel::BoxListModel(QObject* parent)
    : QAbstractTableModel(parent)
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

int BoxListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant BoxListModel::data(const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole || !index.isValid()
        || index.row() < 0 || index.row() >= m_rows.size()
        || index.column() < 0 || index.column() >= ColumnCount) {
        return {};
    }
    const Row& r = m_rows.at(index.row());
    switch (index.column()) {
    case ColNo:
        return index.row() + 1;
    case ColX:
        return r.x;
    case ColY:
        return r.y;
    case ColW:
        return r.width;
    case ColH:
        return r.height;
    case ColArea:
        return qRound(r.area);
    default:
        return {};
    }
}

QVariant BoxListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const QStringList headers = {
        QStringLiteral("序号"),
        QStringLiteral("左"),
        QStringLiteral("上"),
        QStringLiteral("宽"),
        QStringLiteral("高"),
        QStringLiteral("面积"),
    };
    if (section < 0 || section >= headers.size())
        return {};
    return headers.at(section);
}
