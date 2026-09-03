#pragma once

#include "sources/IRejectSink.h"

// 虚拟 DO：NG 打一行 qInfo，便于本阶段看剔除信号。
class LogRejectSink : public IRejectSink
{
public:
    void reject(const RejectContext& ctx) override;
};
