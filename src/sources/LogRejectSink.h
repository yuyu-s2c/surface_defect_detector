#pragma once

#include "sources/IRejectSink.h"

// 控制台剔除日志（[DO] REJECT）。面试看点表请用 SimulatedDoSink，不要把本类当 PLC。
class LogRejectSink : public IRejectSink
{
public:
    void reject(const RejectContext& ctx) override;
};
