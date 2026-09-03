#include "sources/FileRejectSink.h"

#include "ResultExporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QTextStream>

FileRejectSink::FileRejectSink(const QString& sessionDir)
    : m_sessionDir(sessionDir)
    , m_csvPath(sessionDir + QStringLiteral("/rejects.csv"))
{
    QDir().mkpath(m_sessionDir);
    QMutexLocker lock(&m_mutex);
    ensureHeaderLocked();
}

bool FileRejectSink::ensureHeaderLocked()
{
    if (m_headerWritten)
        return true;
    QFile f(m_csvPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << QStringLiteral("category,defect,file,score,threshold,latency_ms,late_eject\n");
    m_headerWritten = true;
    return true;
}

void FileRejectSink::reject(const RejectContext& ctx)
{
    QMutexLocker lock(&m_mutex);
    if (m_sessionDir.isEmpty())
        return;
    QDir().mkpath(m_sessionDir);

    const QString base = QFileInfo(ctx.frame.path).completeBaseName();
    const QString fileName = ctx.frame.defectType.isEmpty()
        ? (base + QStringLiteral("_ann.png"))
        : (ctx.frame.defectType + QLatin1Char('_') + base + QStringLiteral("_ann.png"));
    const QString pngPath = m_sessionDir + QLatin1Char('/') + fileName;
    const cv::Mat annotated = ResultExporter::composeAnnotated(
        ctx.frame.bgr, ctx.result.defectMask, ctx.result.boxes);
    ResultExporter::saveImage(pngPath, annotated);

    if (!ensureHeaderLocked())
        return;
    QFile f(m_csvPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
        return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << ctx.category << ','
        << ctx.frame.defectType << ','
        << QFileInfo(ctx.frame.path).fileName() << ','
        << QString::number(ctx.result.imageScore, 'f', 4) << ','
        << QString::number(ctx.result.imageThreshold, 'f', 4) << ','
        << ctx.latencyMs << ','
        << (ctx.lateEject ? 1 : 0) << '\n';
}
