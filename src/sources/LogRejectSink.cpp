#include "sources/LogRejectSink.h"

#include <QDebug>
#include <QFileInfo>

void LogRejectSink::reject(const RejectContext& ctx)
{
    const QString name = QFileInfo(ctx.frame.path).fileName();
    qInfo().noquote() << QStringLiteral("[DO] REJECT %1/%2/%3 score=%4 thresh=%5 latency=%6 ms%7")
                             .arg(ctx.category, ctx.frame.defectType, name)
                             .arg(ctx.result.imageScore, 0, 'f', 3)
                             .arg(ctx.result.imageThreshold, 0, 'f', 3)
                             .arg(ctx.latencyMs)
                             .arg(ctx.lateEject ? QStringLiteral(" LATE") : QString());
}
