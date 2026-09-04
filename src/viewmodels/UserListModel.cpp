#include "UserListModel.h"

UserListModel::UserListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void UserListModel::setUsers(const QVector<UserRecord>& users)
{
    beginResetModel();
    m_users = users;
    endResetModel();
}

int UserListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_users.size();
}

int UserListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant UserListModel::data(const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole || !index.isValid()
        || index.row() < 0 || index.row() >= m_users.size()
        || index.column() < 0 || index.column() >= ColumnCount) {
        return {};
    }
    const UserRecord& u = m_users.at(index.row());
    switch (index.column()) {
    case ColUsername:
        return u.username;
    case ColDisplayName:
        return u.displayName;
    case ColRole:
        return userRoleLabel(u.role);
    case ColStatus:
        return u.enabled ? QStringLiteral("启用") : QStringLiteral("停用");
    default:
        return {};
    }
}

QVariant UserListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const QStringList headers = {
        QStringLiteral("账号"),
        QStringLiteral("显示名"),
        QStringLiteral("角色"),
        QStringLiteral("状态"),
    };
    if (section < 0 || section >= headers.size())
        return {};
    return headers.at(section);
}

QString UserListModel::usernameAt(int row) const
{
    if (row < 0 || row >= m_users.size())
        return {};
    return m_users.at(row).username;
}

QString UserListModel::displayNameAt(int row) const
{
    if (row < 0 || row >= m_users.size())
        return {};
    return m_users.at(row).displayName;
}

int UserListModel::roleAt(int row) const
{
    if (row < 0 || row >= m_users.size())
        return static_cast<int>(UserRole::Operator);
    return static_cast<int>(m_users.at(row).role);
}

bool UserListModel::enabledAt(int row) const
{
    if (row < 0 || row >= m_users.size())
        return false;
    return m_users.at(row).enabled;
}
