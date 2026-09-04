#pragma once

#include <QByteArray>
#include <QString>

// 本机三角色：操作员只跑检测台；工艺员可进分析台改参数；管理员另管账号。
// 不接域控 / 云账号。CLI（--batch / --live-smoke / --webcam-smoke）不走登录。
enum class UserRole {
    Operator = 0,
    Engineer = 1,
    Admin = 2
};

struct UserRecord {
    QString username;
    QString displayName;
    UserRole role = UserRole::Operator;
    QByteArray salt;
    QString passwordHash;
    bool enabled = true;
};

inline QString userRoleLabel(UserRole role)
{
    switch (role) {
    case UserRole::Engineer:
        return QStringLiteral("工艺员");
    case UserRole::Admin:
        return QStringLiteral("管理员");
    case UserRole::Operator:
    default:
        return QStringLiteral("操作员");
    }
}

inline QString userRoleKey(UserRole role)
{
    switch (role) {
    case UserRole::Engineer:
        return QStringLiteral("engineer");
    case UserRole::Admin:
        return QStringLiteral("admin");
    case UserRole::Operator:
    default:
        return QStringLiteral("operator");
    }
}

inline UserRole userRoleFromKey(const QString& key)
{
    if (key == QStringLiteral("engineer"))
        return UserRole::Engineer;
    if (key == QStringLiteral("admin"))
        return UserRole::Admin;
    return UserRole::Operator;
}
