#pragma once

#include "UserListModel.h"
#include "auth/UserStore.h"

#include <QObject>

// GUI 登录态。QML 绑 `auth`；账号文件在 UserStore，不进 DetectionController。
class AuthViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY sessionChanged)
    Q_PROPERTY(QString username READ username NOTIFY sessionChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY sessionChanged)
    Q_PROPERTY(int role READ role NOTIFY sessionChanged)
    Q_PROPERTY(QString roleLabel READ roleLabel NOTIFY sessionChanged)
    Q_PROPERTY(bool canAnalyze READ canAnalyze NOTIFY sessionChanged)
    Q_PROPERTY(bool canEditParams READ canEditParams NOTIFY sessionChanged)
    Q_PROPERTY(bool canChangeEngine READ canChangeEngine NOTIFY sessionChanged)
    Q_PROPERTY(bool canChangeDataset READ canChangeDataset NOTIFY sessionChanged)
    Q_PROPERTY(bool canEditProcess READ canEditProcess NOTIFY sessionChanged)
    Q_PROPERTY(bool canManageUsers READ canManageUsers NOTIFY sessionChanged)
    Q_PROPERTY(bool seededThisRun READ seededThisRun CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(UserListModel* userModel READ userModel CONSTANT)

public:
    explicit AuthViewModel(QObject* parent = nullptr);

    bool loggedIn() const { return m_loggedIn; }
    QString username() const { return m_username; }
    QString displayName() const { return m_displayName; }
    int role() const { return static_cast<int>(m_role); }
    QString roleLabel() const;
    bool canAnalyze() const { return m_loggedIn && m_role != UserRole::Operator; }
    bool canEditParams() const { return canAnalyze(); }
    bool canChangeEngine() const { return canAnalyze(); }
    bool canChangeDataset() const { return canAnalyze(); }
    bool canEditProcess() const { return canAnalyze(); }
    bool canManageUsers() const { return m_loggedIn && m_role == UserRole::Admin; }
    bool seededThisRun() const { return m_store.seededThisRun(); }
    QString lastError() const { return m_lastError; }
    QString lastMessage() const { return m_lastMessage; }
    UserListModel* userModel() const { return m_userModel; }

    Q_INVOKABLE bool login(const QString& username, const QString& password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE bool changeOwnPassword(const QString& oldPassword, const QString& newPassword);
    Q_INVOKABLE bool addUser(const QString& username, const QString& displayName,
                             const QString& password, int role);
    Q_INVOKABLE bool setUserEnabled(const QString& username, bool enabled);
    Q_INVOKABLE bool setUserRole(const QString& username, int role);
    Q_INVOKABLE bool setUserDisplayName(const QString& username, const QString& displayName);
    Q_INVOKABLE bool resetPassword(const QString& username, const QString& password);
    Q_INVOKABLE bool removeUser(const QString& username);
    Q_INVOKABLE void clearError();

signals:
    void sessionChanged();
    void lastErrorChanged();
    void lastMessageChanged();

private:
    UserRole clampRole(int role) const;
    void setError(const QString& msg);
    void setMessage(const QString& msg);
    void refreshUsers();
    void applySession(const UserRecord& user);
    void clearSession();

    UserStore m_store;
    UserListModel* m_userModel = nullptr;
    bool m_loggedIn = false;
    UserRole m_role = UserRole::Operator;
    QString m_username;
    QString m_displayName;
    QString m_lastError;
    QString m_lastMessage;
};
