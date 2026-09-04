#include "UserStore.h"
#include "log/AppLog.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr int kMinPasswordLen = 4;
const QRegularExpression kUserRe(QStringLiteral("^[a-z][a-z0-9_]{1,31}$"));

QString hexOf(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toHex());
}

QByteArray bytesFromHex(const QString& hex)
{
    return QByteArray::fromHex(hex.toLatin1());
}

} // namespace

UserStore::UserStore()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(root);
    m_path = QDir(root).filePath(QStringLiteral("users.json"));
}

bool UserStore::load()
{
    m_users.clear();
    m_seededThisRun = false;

    QFile f(m_path);
    if (!f.exists()) {
        seedDefaults();
        m_seededThisRun = true;
        qCInfo(lcAuth) << "账号簿不存在，写入默认账号" << m_path;
        return save();
    }
    if (!f.open(QIODevice::ReadOnly)) {
        qCWarning(lcAuth) << "无法读取账号簿" << m_path;
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        qCWarning(lcAuth) << "账号簿格式无效" << m_path;
        return false;
    }

    const QJsonArray arr = doc.object().value(QStringLiteral("users")).toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        UserRecord u;
        u.username = normalizeUsername(o.value(QStringLiteral("username")).toString());
        if (!isValidUsername(u.username))
            continue;
        u.displayName = o.value(QStringLiteral("displayName")).toString().trimmed();
        if (u.displayName.isEmpty())
            u.displayName = u.username;
        u.role = userRoleFromKey(o.value(QStringLiteral("role")).toString());
        u.salt = bytesFromHex(o.value(QStringLiteral("salt")).toString());
        u.passwordHash = o.value(QStringLiteral("passwordHash")).toString();
        u.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        if (u.salt.size() != 16 || u.passwordHash.size() != 64)
            continue;
        m_users.push_back(u);
    }

    if (m_users.isEmpty()) {
        seedDefaults();
        m_seededThisRun = true;
        qCWarning(lcAuth) << "账号簿无有效用户，重新写入默认账号" << m_path;
        return save();
    }
    qCInfo(lcAuth) << "账号簿" << m_path << "用户数" << m_users.size();
    return true;
}

bool UserStore::save() const
{
    QJsonArray arr;
    for (const UserRecord& u : m_users) {
        QJsonObject o;
        o.insert(QStringLiteral("username"), u.username);
        o.insert(QStringLiteral("displayName"), u.displayName);
        o.insert(QStringLiteral("role"), userRoleKey(u.role));
        o.insert(QStringLiteral("salt"), hexOf(u.salt));
        o.insert(QStringLiteral("passwordHash"), u.passwordHash);
        o.insert(QStringLiteral("enabled"), u.enabled);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("users"), arr);

    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly)) {
        qCWarning(lcAuth) << "无法写入账号簿" << m_path;
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

const UserRecord* UserStore::find(const QString& username) const
{
    return constFind(username);
}

UserRecord* UserStore::find(const QString& username)
{
    return mutableFind(username);
}

bool UserStore::verifyPassword(const UserRecord& user, const QString& password) const
{
    if (user.salt.size() != 16 || user.passwordHash.size() != 64)
        return false;
    return hashPassword(user.salt, password) == user.passwordHash;
}

QString UserStore::addUser(const QString& username, const QString& displayName,
                           const QString& password, UserRole role)
{
    const QString name = normalizeUsername(username);
    if (!isValidUsername(name))
        return QStringLiteral("账号须以字母开头，仅字母数字下划线，2～32 位");
    if (constFind(name))
        return QStringLiteral("账号已存在");
    if (!isValidPassword(password))
        return QStringLiteral("口令至少 %1 位").arg(kMinPasswordLen);

    UserRecord u;
    u.username = name;
    u.displayName = displayName.trimmed();
    if (u.displayName.isEmpty())
        u.displayName = name;
    u.role = role;
    u.salt = makeSalt();
    u.passwordHash = hashPassword(u.salt, password);
    u.enabled = true;
    m_users.push_back(u);
    if (!save()) {
        m_users.removeLast();
        return persistError(QStringLiteral("新增"));
    }
    return {};
}

QString UserStore::setEnabled(const QString& username, bool enabled)
{
    UserRecord* u = mutableFind(username);
    if (!u)
        return QStringLiteral("账号不存在");
    if (!enabled && u->role == UserRole::Admin && enabledAdminCount() <= 1)
        return QStringLiteral("不能停用最后一位启用的管理员");
    if (u->enabled == enabled)
        return {};
    const bool prev = u->enabled;
    u->enabled = enabled;
    if (!save()) {
        u->enabled = prev;
        return persistError(QStringLiteral("改状态"));
    }
    return {};
}

QString UserStore::setRole(const QString& username, UserRole role)
{
    UserRecord* u = mutableFind(username);
    if (!u)
        return QStringLiteral("账号不存在");
    if (u->role == UserRole::Admin && role != UserRole::Admin && enabledAdminCount() <= 1)
        return QStringLiteral("不能降级最后一位启用的管理员");
    const UserRole prev = u->role;
    u->role = role;
    if (!save()) {
        u->role = prev;
        return persistError(QStringLiteral("改角色"));
    }
    return {};
}

QString UserStore::setPassword(const QString& username, const QString& password)
{
    UserRecord* u = mutableFind(username);
    if (!u)
        return QStringLiteral("账号不存在");
    if (!isValidPassword(password))
        return QStringLiteral("口令至少 %1 位").arg(kMinPasswordLen);
    const QByteArray prevSalt = u->salt;
    const QString prevHash = u->passwordHash;
    u->salt = makeSalt();
    u->passwordHash = hashPassword(u->salt, password);
    if (!save()) {
        u->salt = prevSalt;
        u->passwordHash = prevHash;
        return persistError(QStringLiteral("改口令"));
    }
    return {};
}

QString UserStore::setDisplayName(const QString& username, const QString& displayName)
{
    UserRecord* u = mutableFind(username);
    if (!u)
        return QStringLiteral("账号不存在");
    QString name = displayName.trimmed();
    if (name.isEmpty())
        name = u->username;
    const QString prev = u->displayName;
    u->displayName = name;
    if (!save()) {
        u->displayName = prev;
        return persistError(QStringLiteral("改显示名"));
    }
    return {};
}

QString UserStore::removeUser(const QString& username, const QString& actingUser)
{
    const QString name = normalizeUsername(username);
    if (name == normalizeUsername(actingUser))
        return QStringLiteral("不能删除当前登录账号");
    UserRecord* u = mutableFind(name);
    if (!u)
        return QStringLiteral("账号不存在");
    if (u->role == UserRole::Admin && u->enabled && enabledAdminCount() <= 1)
        return QStringLiteral("不能删除最后一位启用的管理员");

    int idx = -1;
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users.at(i).username == name) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return QStringLiteral("账号不存在");
    const UserRecord copy = m_users.at(idx);
    m_users.removeAt(idx);
    if (!save()) {
        m_users.insert(idx, copy);
        return persistError(QStringLiteral("删除"));
    }
    return {};
}

QString UserStore::normalizeUsername(const QString& raw)
{
    return raw.trimmed().toLower();
}

bool UserStore::isValidUsername(const QString& username)
{
    return kUserRe.match(username).hasMatch();
}

bool UserStore::isValidPassword(const QString& password)
{
    return password.size() >= kMinPasswordLen;
}

UserRecord* UserStore::mutableFind(const QString& username)
{
    const QString name = normalizeUsername(username);
    for (UserRecord& u : m_users) {
        if (u.username == name)
            return &u;
    }
    return nullptr;
}

const UserRecord* UserStore::constFind(const QString& username) const
{
    const QString name = normalizeUsername(username);
    for (const UserRecord& u : m_users) {
        if (u.username == name)
            return &u;
    }
    return nullptr;
}

int UserStore::enabledAdminCount() const
{
    int n = 0;
    for (const UserRecord& u : m_users) {
        if (u.enabled && u.role == UserRole::Admin)
            ++n;
    }
    return n;
}

void UserStore::seedDefaults()
{
    m_users.clear();
    auto add = [this](const QString& name, const QString& display, UserRole role) {
        UserRecord u;
        u.username = name;
        u.displayName = display;
        u.role = role;
        u.salt = makeSalt();
        u.passwordHash = hashPassword(u.salt, name);
        u.enabled = true;
        m_users.push_back(u);
    };
    add(QStringLiteral("admin"), QStringLiteral("管理员"), UserRole::Admin);
    add(QStringLiteral("engineer"), QStringLiteral("工艺员"), UserRole::Engineer);
    add(QStringLiteral("operator"), QStringLiteral("操作员"), UserRole::Operator);
}

QByteArray UserStore::makeSalt()
{
    QByteArray salt(16, 0);
    auto* rng = QRandomGenerator::system();
    for (int i = 0; i < salt.size(); ++i)
        salt[i] = static_cast<char>(rng->bounded(256));
    return salt;
}

QString UserStore::hashPassword(const QByteArray& salt, const QString& password)
{
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(salt);
    h.addData(password.toUtf8());
    return QString::fromLatin1(h.result().toHex());
}

QString UserStore::persistError(const QString& action)
{
    return QStringLiteral("%1失败，无法写入账号文件").arg(action);
}
