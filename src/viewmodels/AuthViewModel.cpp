#include "AuthViewModel.h"
#include "log/AppLog.h"

AuthViewModel::AuthViewModel(QObject* parent)
    : QObject(parent)
    , m_userModel(new UserListModel(this))
{
    m_store.load();
    refreshUsers();
}

QString AuthViewModel::roleLabel() const
{
    if (!m_loggedIn)
        return {};
    return userRoleLabel(m_role);
}

bool AuthViewModel::login(const QString& username, const QString& password)
{
    const QString name = UserStore::normalizeUsername(username);
    if (name.isEmpty() || password.isEmpty()) {
        setError(QStringLiteral("请输入账号和口令"));
        return false;
    }
    const UserRecord* u = m_store.find(name);
    if (!u || !m_store.verifyPassword(*u, password)) {
        qCWarning(lcAuth) << "登录失败" << name << "口令错误或不存在";
        setError(QStringLiteral("账号或口令不正确"));
        return false;
    }
    if (!u->enabled) {
        qCWarning(lcAuth) << "登录失败" << name << "账号已停用";
        setError(QStringLiteral("账号已停用"));
        return false;
    }
    applySession(*u);
    qCInfo(lcAuth) << "登录" << u->username << userRoleKey(u->role);
    setError({});
    setMessage({});
    return true;
}

void AuthViewModel::logout()
{
    if (!m_loggedIn)
        return;
    qCInfo(lcAuth) << "登出" << m_username << userRoleKey(m_role);
    clearSession();
    setError({});
    setMessage({});
}

bool AuthViewModel::changeOwnPassword(const QString& oldPassword, const QString& newPassword)
{
    if (!m_loggedIn) {
        setError(QStringLiteral("尚未登录"));
        return false;
    }
    const UserRecord* u = m_store.find(m_username);
    if (!u || !m_store.verifyPassword(*u, oldPassword)) {
        setError(QStringLiteral("当前口令不正确"));
        return false;
    }
    const QString err = m_store.setPassword(m_username, newPassword);
    if (!err.isEmpty()) {
        setError(err);
        return false;
    }
    qCInfo(lcAuth) << "口令已更新" << m_username;
    setError({});
    setMessage(QStringLiteral("口令已更新"));
    return true;
}

bool AuthViewModel::addUser(const QString& username, const QString& displayName,
                            const QString& password, int role)
{
    if (!canManageUsers()) {
        setError(QStringLiteral("只有管理员能管账号"));
        return false;
    }
    const QString err = m_store.addUser(username, displayName, password, clampRole(role));
    if (!err.isEmpty()) {
        setError(err);
        return false;
    }
    refreshUsers();
    qCInfo(lcAuth) << "新增账号" << UserStore::normalizeUsername(username)
                   << userRoleKey(clampRole(role));
    setError({});
    setMessage(QStringLiteral("已新增 %1").arg(UserStore::normalizeUsername(username)));
    return true;
}

bool AuthViewModel::setUserEnabled(const QString& username, bool enabled)
{
    if (!canManageUsers()) {
        setError(QStringLiteral("只有管理员能管账号"));
        return false;
    }
    if (UserStore::normalizeUsername(username) == m_username && !enabled) {
        setError(QStringLiteral("不能停用当前登录账号"));
        return false;
    }
    const QString err = m_store.setEnabled(username, enabled);
    if (!err.isEmpty()) {
        setError(err);
        return false;
    }
    refreshUsers();
    qCInfo(lcAuth) << "账号" << UserStore::normalizeUsername(username)
                   << (enabled ? "启用" : "停用");
    setError({});
    setMessage(enabled ? QStringLiteral("已启用") : QStringLiteral("已停用"));
    return true;
}

bool AuthViewModel::setUserRole(const QString& username, int role)
{
    if (!canManageUsers()) {
        setError(QStringLiteral("只有管理员能管账号"));
        return false;
    }
    const UserRole next = clampRole(role);
    if (UserStore::normalizeUsername(username) == m_username && next != UserRole::Admin) {
        setError(QStringLiteral("不能降级当前登录账号"));
        return false;
    }
    const QString err = m_store.setRole(username, next);
    if (!err.isEmpty()) {
        setError(err);
        return false;
    }
    refreshUsers();
    qCInfo(lcAuth) << "账号角色" << UserStore::normalizeUsername(username)
                   << userRoleKey(next);
    setError({});
    setMessage(QStringLiteral("角色已更新"));
    return true;
}

bool AuthViewModel::setUserDisplayName(const QString& username, const QString& displayName)
{
    if (!canManageUsers()) {
        setError(QStringLiteral("只有管理员能管账号"));
        return false;
    }
    const QString err = m_store.setDisplayName(username, displayName);
    if (!err.isEmpty()) {
        setError(err);
        return false;
    }
    if (UserStore::normalizeUsername(username) == m_username) {
        m_displayName = displayName.trimmed().isEmpty()
                            ? m_username
                            : displayName.trimmed();
        emit sessionChanged();
    }
    refreshUsers();
    setError({});
    setMessage(QStringLiteral("显示名已更新"));
    return true;
}

bool AuthViewModel::resetPassword(const QString& username, const QString& password)
{
    if (!canManageUsers()) {
        setError(QStringLiteral("只有管理员能管账号"));
        return false;
    }
    const QString err = m_store.setPassword(username, password);
    if (!err.isEmpty()) {
        setError(err);
        return false;
    }
    refreshUsers();
    qCInfo(lcAuth) << "口令已重置" << UserStore::normalizeUsername(username);
    setError({});
    setMessage(QStringLiteral("口令已重置"));
    return true;
}

bool AuthViewModel::removeUser(const QString& username)
{
    if (!canManageUsers()) {
        setError(QStringLiteral("只有管理员能管账号"));
        return false;
    }
    const QString err = m_store.removeUser(username, m_username);
    if (!err.isEmpty()) {
        setError(err);
        return false;
    }
    refreshUsers();
    qCInfo(lcAuth) << "删除账号" << UserStore::normalizeUsername(username);
    setError({});
    setMessage(QStringLiteral("已删除"));
    return true;
}

void AuthViewModel::clearError()
{
    setError({});
    setMessage({});
}

UserRole AuthViewModel::clampRole(int role) const
{
    if (role == static_cast<int>(UserRole::Admin))
        return UserRole::Admin;
    if (role == static_cast<int>(UserRole::Engineer))
        return UserRole::Engineer;
    return UserRole::Operator;
}

void AuthViewModel::setError(const QString& msg)
{
    if (m_lastError == msg)
        return;
    m_lastError = msg;
    emit lastErrorChanged();
}

void AuthViewModel::setMessage(const QString& msg)
{
    if (m_lastMessage == msg)
        return;
    m_lastMessage = msg;
    emit lastMessageChanged();
}

void AuthViewModel::refreshUsers()
{
    m_userModel->setUsers(m_store.users());
}

void AuthViewModel::applySession(const UserRecord& user)
{
    m_loggedIn = true;
    m_username = user.username;
    m_displayName = user.displayName;
    m_role = user.role;
    emit sessionChanged();
}

void AuthViewModel::clearSession()
{
    m_loggedIn = false;
    m_username.clear();
    m_displayName.clear();
    m_role = UserRole::Operator;
    emit sessionChanged();
}
