#include "sources/CameraSource.h"

CameraSource::CameraSource(QObject* parent)
    : IFrameSource(parent)
{
}

bool CameraSource::start()
{
    emit errorOccurred(QStringLiteral("未接相机（Phase 4）"));
    return false;
}

void CameraSource::stop() {}

bool CameraSource::grab(CapturedFrame& out)
{
    Q_UNUSED(out);
    return false;
}

bool CameraSource::isRunning() const
{
    return false;
}

QueueOverflowPolicy CameraSource::overflowPolicy() const
{
    // P4 填海康后 Session 按此丢最旧帧。本阶段 start() 仍失败。
    return QueueOverflowPolicy::DropOldest;
}
