#include "sources/SimulatedDoSink.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QTextStream>
#include <QtDebug>

SimulatedDoSink::SimulatedDoSink(const QString& sessionDir)
    : m_sessionDir(sessionDir)
    , m_mapPath(sessionDir + QStringLiteral("/do_map.csv"))
    , m_csvPath(sessionDir + QStringLiteral("/do_pulses.csv"))
{
    if (m_sessionDir.isEmpty())
        return;
    QDir().mkpath(m_sessionDir);
    QMutexLocker lock(&m_mutex);
    writePointTableLocked();
    ensurePulseHeaderLocked();
}

bool SimulatedDoSink::writePointTableLocked()
{
    if (m_mapWritten)
        return true;
    QFile f(m_mapPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    // 表头注明 simulated：打开文件就能看出不是实 PLC 报文
    out << QStringLiteral("# simulated PLC/DO point table — not a real fieldbus\n");
    out << QStringLiteral("point,name,kind,pulse_ms,note\n");
    out << QStringLiteral("DO0.0,REJECT,pulse,100,模拟剔除气缸（P4 换成实 DO）\n");
    out << QStringLiteral("DO0.1,LATE,level,0,迟剔除告警灯（仅模拟）\n");
    m_mapWritten = true;
    return true;
}

bool SimulatedDoSink::ensurePulseHeaderLocked()
{
    if (m_headerWritten)
        return true;
    QFile f(m_csvPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << QStringLiteral("seq,point,action,pulse_ms,category,defect,file,score,threshold,latency_ms,late\n");
    m_headerWritten = true;
    return true;
}

void SimulatedDoSink::reject(const RejectContext& ctx)
{
    QMutexLocker lock(&m_mutex);
    if (m_sessionDir.isEmpty())
        return;
    QDir().mkpath(m_sessionDir);
    writePointTableLocked();
    if (!ensurePulseHeaderLocked())
        return;

    const QString name = QFileInfo(ctx.frame.path).fileName();
    ++m_seq;

    QFile f(m_csvPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
        return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << m_seq << ','
        << QString::fromLatin1(kRejectPoint) << ','
        << QStringLiteral("REJECT") << ','
        << kPulseMs << ','
        << ctx.category << ','
        << ctx.frame.defectType << ','
        << name << ','
        << QString::number(ctx.result.imageScore, 'f', 4) << ','
        << QString::number(ctx.result.imageThreshold, 'f', 4) << ','
        << ctx.latencyMs << ','
        << (ctx.lateEject ? 1 : 0) << '\n';

    qInfo().noquote() << QStringLiteral("[PLC-SIM] %1 PULSE %2ms REJECT %3/%4/%5 score=%6 thresh=%7 latency=%8 ms%9")
                             .arg(QString::fromLatin1(kRejectPoint))
                             .arg(kPulseMs)
                             .arg(ctx.category, ctx.frame.defectType, name)
                             .arg(ctx.result.imageScore, 0, 'f', 3)
                             .arg(ctx.result.imageThreshold, 0, 'f', 3)
                             .arg(ctx.latencyMs)
                             .arg(ctx.lateEject ? QStringLiteral(" LATE") : QString());

    if (ctx.lateEject) {
        qInfo().noquote() << QStringLiteral("[PLC-SIM] %1 ON LATE-ALARM %2/%3/%4")
                                 .arg(QString::fromLatin1(kLatePoint),
                                      ctx.category, ctx.frame.defectType, name);
    }
}
