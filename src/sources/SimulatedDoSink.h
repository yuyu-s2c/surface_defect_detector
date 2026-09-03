#pragma once

#include "sources/IRejectSink.h"

#include <QMutex>
#include <QString>

// 面试可见的模拟 PLC/DO：点表 + 脉冲日志落盘，不是实现场总线。
// P4 到货后另写真实 DO 的 IRejectSink 加进 Composite，不要把本类改成真协议。
class SimulatedDoSink : public IRejectSink
{
public:
    // 模拟点表（写进 do_map.csv，便于打开班次目录对照）
    static constexpr const char* kRejectPoint = "DO0.0";
    static constexpr const char* kLatePoint = "DO0.1";
    static constexpr int kPulseMs = 100;

    explicit SimulatedDoSink(const QString& sessionDir);

    void reject(const RejectContext& ctx) override;

private:
    bool writePointTableLocked();
    bool ensurePulseHeaderLocked();

    QString m_sessionDir;
    QString m_mapPath;
    QString m_csvPath;
    bool m_mapWritten = false;
    bool m_headerWritten = false;
    int m_seq = 0;
    QMutex m_mutex;
};
