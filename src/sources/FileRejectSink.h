#pragma once

#include "sources/IRejectSink.h"

#include <QMutex>
#include <QString>

// NG 叠图 + rejects.csv 落到班次目录。检测线程调用，内部加锁。
class FileRejectSink : public IRejectSink
{
public:
    explicit FileRejectSink(const QString& sessionDir);

    void reject(const RejectContext& ctx) override;
    QString sessionDir() const { return m_sessionDir; }

private:
    bool ensureHeaderLocked();

    QString m_sessionDir;
    QString m_csvPath;
    bool m_headerWritten = false;
    QMutex m_mutex;
};
