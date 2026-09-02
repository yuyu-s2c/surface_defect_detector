#include "sources/LogRejectSink.h"

#include <QDebug>
#include <QFileInfo>

void LogRejectSink::reject(const QString& category, const CapturedFrame& frame,
                           const DetectionResult& result)
{
    const QString name = QFileInfo(frame.path).fileName();
    qInfo().noquote() << QStringLiteral("[DO] REJECT %1/%2/%3 score=%4 thresh=%5")
                             .arg(category, frame.defectType, name)
                             .arg(result.imageScore, 0, 'f', 3)
                             .arg(result.imageThreshold, 0, 'f', 3);
}
