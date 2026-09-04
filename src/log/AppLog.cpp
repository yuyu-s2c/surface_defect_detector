#include "log/AppLog.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QtGlobal>

#include <cstdio>
#include <cstdlib>

Q_LOGGING_CATEGORY(lcApp, "app")
Q_LOGGING_CATEGORY(lcAuth, "app.auth")
Q_LOGGING_CATEGORY(lcEngine, "app.engine")
Q_LOGGING_CATEGORY(lcSession, "app.session")
Q_LOGGING_CATEGORY(lcSource, "app.source")
Q_LOGGING_CATEGORY(lcReject, "app.reject")
Q_LOGGING_CATEGORY(lcGui, "app.gui")

namespace {

QMutex g_mutex;
QFile g_file;
QTextStream g_stream;
QString g_path;
QDate g_fileDate;
bool g_explicitPath = false;
bool g_stderr = true;
bool g_installed = false;

thread_local bool t_inHandler = false;

const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }
    return "????";
}

QString shortFile(const char* file)
{
    if (!file || !file[0])
        return QStringLiteral("-");
    const QString s = QString::fromUtf8(file);
    const int slash = qMax(s.lastIndexOf(u'/'), s.lastIndexOf(u'\\'));
    return slash >= 0 ? s.mid(slash + 1) : s;
}

QString defaultDailyPath()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dir = QDir(root).filePath(QStringLiteral("logs"));
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("sdd-%1.log")
                                  .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
}

bool openFile(const QString& path)
{
    if (g_file.isOpen()) {
        g_stream.flush();
        g_file.close();
    }
    g_path = path;
    QDir().mkpath(QFileInfo(path).absolutePath());
    g_file.setFileName(path);
    if (!g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        g_stream.setDevice(nullptr);
        return false;
    }
    g_stream.setDevice(&g_file);
    g_stream.setEncoding(QStringConverter::Utf8);
    g_fileDate = QDate::currentDate();
    return true;
}

void rotateIfNeeded()
{
    if (g_explicitPath)
        return;
    const QDate today = QDate::currentDate();
    if (g_file.isOpen() && g_fileDate == today)
        return;
    openFile(defaultDailyPath());
}

QString formatLine(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    const QString cat = (ctx.category && ctx.category[0])
        ? QString::fromUtf8(ctx.category)
        : QStringLiteral("default");
    const quintptr tid = reinterpret_cast<quintptr>(QThread::currentThreadId());
    return QStringLiteral("%1 %2 %3 %4 %5:%6 %7")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz")),
             QLatin1String(levelName(type)),
             cat,
             QString::number(tid, 16),
             shortFile(ctx.file))
        .arg(ctx.line)
        .arg(msg);
}

void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (t_inHandler)
        return;
    t_inHandler = true;

    const QString line = formatLine(type, ctx, msg);
    {
        QMutexLocker lock(&g_mutex);
        rotateIfNeeded();
        if (g_stream.device()) {
            g_stream << line << '\n';
            if (type >= QtWarningMsg)
                g_stream.flush();
        }
    }

    if (g_stderr) {
        const QByteArray utf8 = line.toUtf8();
        std::fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stderr);
        std::fputc('\n', stderr);
        if (type >= QtWarningMsg)
            std::fflush(stderr);
    }

    t_inHandler = false;
    if (type == QtFatalMsg) {
        AppLog::shutdown();
        std::abort();
    }
}

void applyFilter(AppLog::Level level)
{
    switch (level) {
    case AppLog::Level::Debug:
        QLoggingCategory::setFilterRules(QStringLiteral(
            "*.debug=false\n"
            "app.debug=true\n"
            "app.*.debug=true\n"));
        break;
    case AppLog::Level::Warning:
        QLoggingCategory::setFilterRules(QStringLiteral(
            "*.debug=false\n"
            "*.info=false\n"));
        break;
    case AppLog::Level::Info:
    default:
        QLoggingCategory::setFilterRules(QStringLiteral(
            "*.debug=false\n"
            "app.info=true\n"
            "app.*.info=true\n"));
        break;
    }
}

} // namespace

bool AppLog::parseOptions(const QStringList& args, Options* out, QString* error)
{
    Options opt;
    const int levelIdx = args.indexOf(QStringLiteral("--log-level"));
    if (levelIdx >= 0) {
        if (levelIdx + 1 >= args.size()) {
            if (error)
                *error = QStringLiteral("用法: --log-level debug|info|warning");
            return false;
        }
        const QString v = args.at(levelIdx + 1);
        if (v == QStringLiteral("debug"))
            opt.minLevel = Level::Debug;
        else if (v == QStringLiteral("info"))
            opt.minLevel = Level::Info;
        else if (v == QStringLiteral("warning"))
            opt.minLevel = Level::Warning;
        else {
            if (error)
                *error = QStringLiteral("未知 --log-level: %1（可选 debug|info|warning）").arg(v);
            return false;
        }
        opt.levelFromArgs = true;
    }

    const int fileIdx = args.indexOf(QStringLiteral("--log-file"));
    if (fileIdx >= 0) {
        if (fileIdx + 1 >= args.size()) {
            if (error)
                *error = QStringLiteral("用法: --log-file <路径>");
            return false;
        }
        opt.filePath = args.at(fileIdx + 1);
        if (opt.filePath.isEmpty() || opt.filePath.startsWith(QLatin1Char('-'))) {
            if (error)
                *error = QStringLiteral("用法: --log-file <路径>");
            return false;
        }
    }

    if (out)
        *out = opt;
    return true;
}

void AppLog::install(const Options& opt)
{
    const bool applyRules = opt.levelFromArgs
        || qEnvironmentVariableIsEmpty("QT_LOGGING_RULES");
    if (applyRules)
        applyFilter(opt.minLevel);

    QString path = opt.filePath;
    g_explicitPath = !path.isEmpty();
    if (path.isEmpty()) {
        path = defaultDailyPath();
    } else {
        path = QFileInfo(path).absoluteFilePath();
        QDir().mkpath(QFileInfo(path).absolutePath());
    }

    {
        QMutexLocker lock(&g_mutex);
        g_stderr = opt.alsoStderr;
        openFile(path);
    }

    if (!g_installed) {
        qInstallMessageHandler(messageHandler);
        qAddPostRoutine([]() { shutdown(); });
        g_installed = true;
    }

    qCInfo(lcApp) << "日志文件" << currentFilePath();
}

QString AppLog::currentFilePath()
{
    QMutexLocker lock(&g_mutex);
    return g_path;
}

void AppLog::shutdown()
{
    QMutexLocker lock(&g_mutex);
    if (g_stream.device())
        g_stream.flush();
    if (g_file.isOpen())
        g_file.close();
    g_stream.setDevice(nullptr);
}
