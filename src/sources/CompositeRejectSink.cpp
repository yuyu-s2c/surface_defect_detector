#include "sources/CompositeRejectSink.h"

void CompositeRejectSink::add(std::unique_ptr<IRejectSink> sink)
{
    if (sink)
        m_sinks.push_back(std::move(sink));
}

void CompositeRejectSink::reject(const RejectContext& ctx)
{
    for (auto& sink : m_sinks)
        sink->reject(ctx);
}
