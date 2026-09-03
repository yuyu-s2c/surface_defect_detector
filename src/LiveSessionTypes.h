#pragma once

#include "IDetectionEngine.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

// 取流班次的一条工件记录（不含图，给 session.csv / 回看索引）。
struct LivePieceRecord
{
    QString category;
    QString defectType;
    QString path;
    bool detected = false;
    double imageScore = 0.0;
    double imageThreshold = 0.0;
    qint64 latencyMs = 0;
    bool lateEject = false;
    int queueDepth = 0;
};

// 一次 start()～finished() 的摘要。P4 接相机后字段不用改。
struct LiveSessionSummary
{
    QString sessionId;
    QString sessionDir;
    QString category;
    QString engineName;
    QString provider;
    int targetFps = 0;
    double effectiveFps = 0.0;
    double elapsedSec = 0.0;
    int ok = 0;
    int ng = 0;
    int dropped = 0;
    int lateEject = 0;
    int maxQueue = 0;
    qint64 maxLatencyMs = 0;
    int planned = 0;
    int done = 0;
};
