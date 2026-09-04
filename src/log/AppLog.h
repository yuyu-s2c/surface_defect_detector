#pragma once

#include <QLoggingCategory>
#include <QString>
#include <QStringList>

// 诊断分类。新模块声明一个 Q_LOGGING_CATEGORY 即可，不要再裸 qDebug。
// 口令 / users.json 盐哈希 / 每帧检测结果不要走 info。
Q_DECLARE_LOGGING_CATEGORY(lcApp)
Q_DECLARE_LOGGING_CATEGORY(lcAuth)
Q_DECLARE_LOGGING_CATEGORY(lcEngine)
Q_DECLARE_LOGGING_CATEGORY(lcSession)
Q_DECLARE_LOGGING_CATEGORY(lcSource)
Q_DECLARE_LOGGING_CATEGORY(lcReject)
Q_DECLARE_LOGGING_CATEGORY(lcGui)

namespace AppLog {

enum class Level { Debug, Info, Warning };

struct Options {
    Level minLevel = Level::Info;
    bool levelFromArgs = false; // --log-level 优先于 QT_LOGGING_RULES
    QString filePath;           // 空则 AppData/logs/sdd-yyyy-MM-dd.log，按日切换
    bool alsoStderr = true;
};

// 解析 --log-level debug|info|warning 与 --log-file <path>。非法返回 false。
bool parseOptions(const QStringList& args, Options* out, QString* error = nullptr);

// 须在 setOrganizationName / setApplicationName 之后调用。
void install(const Options& opt = {});
QString currentFilePath();
void shutdown();

} // namespace AppLog
