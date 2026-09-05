#pragma once

#include "UserTypes.h"

#include <QString>
#include <QVector>

// 本机账号簿：AppData/users.json。密码 SHA-256(盐 + UTF-8 密码)，不入库、不进 QSettings。
// 文件不存在时种 admin / engineer / operator（初始密码 123456，方便本机测试）。
class UserStore
{
public:
    UserStore();

    bool load();
    bool save() const;

    QString filePath() const { return m_path; }
    bool seededThisRun() const { return m_seededThisRun; }

    QVector<UserRecord> users() const { return m_users; }
    const UserRecord* find(const QString& username) const;
    UserRecord* find(const QString& username);

    bool verifyPassword(const UserRecord& user, const QString& password) const;

    QString addUser(const QString& username, const QString& displayName,
                    const QString& password, UserRole role);
    QString setEnabled(const QString& username, bool enabled);
    QString setRole(const QString& username, UserRole role);
    QString setPassword(const QString& username, const QString& password);
    QString setDisplayName(const QString& username, const QString& displayName);
    QString removeUser(const QString& username, const QString& actingUser);

    static QString normalizeUsername(const QString& raw);
    static bool isValidUsername(const QString& username);
    static bool isValidPassword(const QString& password);

private:
    UserRecord* mutableFind(const QString& username);
    const UserRecord* constFind(const QString& username) const;
    int enabledAdminCount() const;
    void seedDefaults();
    static QByteArray makeSalt();
    static QString hashPassword(const QByteArray& salt, const QString& password);
    static QString persistError(const QString& action);

    QString m_path;
    QVector<UserRecord> m_users;
    bool m_seededThisRun = false;
};
