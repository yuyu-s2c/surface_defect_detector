#pragma once

#include "IDetectionEngine.h"
#include "sources/IFrameSource.h"

#include <QString>

// 一次剔除调用的上下文。日志 / 落盘 / P4 实 DO 共用，避免再给接口加参数。
struct RejectContext
{
    QString category;
    CapturedFrame frame;
    DetectionResult result;
    qint64 latencyMs = 0;
    bool lateEject = false;
};

// 剔除机构抽象。本阶段：日志 + 落盘 + 模拟 PLC/DO 点表。
// P4 再实现一个真实 DO 的 IRejectSink 加进 CompositeRejectSink，不改 InspectionSession。
class IRejectSink
{
public:
    virtual ~IRejectSink() = default;
    virtual void reject(const RejectContext& ctx) = 0;
};
