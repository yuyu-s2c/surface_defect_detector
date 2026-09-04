#pragma once

#include "auth/UserTypes.h"

#include <QAbstractTableModel>
#include <QVector>

class UserListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColUsername = 0,
        ColDisplayName,
        ColRole,
        ColStatus,
        ColumnCount
    };

    explicit UserListModel(QObject* parent = nullptr);

    void setUsers(const QVector<UserRecord>& users);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    Q_INVOKABLE QString usernameAt(int row) const;
    Q_INVOKABLE QString displayNameAt(int row) const;
    Q_INVOKABLE int roleAt(int row) const;
    Q_INVOKABLE bool enabledAt(int row) const;

private:
    QVector<UserRecord> m_users;
};
