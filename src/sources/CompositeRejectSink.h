#pragma once

#include "sources/IRejectSink.h"

#include <memory>
#include <vector>

// 日志 + 落盘并行。P4 真 DO 作为再一个 sink 加进来即可。
class CompositeRejectSink : public IRejectSink
{
public:
    void add(std::unique_ptr<IRejectSink> sink);
    void reject(const RejectContext& ctx) override;

private:
    std::vector<std::unique_ptr<IRejectSink>> m_sinks;
};
