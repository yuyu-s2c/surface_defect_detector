#pragma once

#include "IDetectionEngine.h"
#include "sources/IFrameSource.h"

#include <QString>

// 剔除机构抽象。本阶段打日志；P4 换成实 DO，不改 InspectionSession。
class IRejectSink
{
public:
    virtual ~IRejectSink() = default;
    virtual void reject(const QString& category, const CapturedFrame& frame,
                        const DetectionResult& result) = 0;
};
