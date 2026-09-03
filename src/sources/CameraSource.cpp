#include "sources/CameraSource.h"

CameraSource::CameraSource(QObject* parent)
    : IFrameSource(parent)
{
}

bool CameraSource::start()
{
    emit errorOccurred(QStringLiteral("相机离线（Phase 4）。本工位不发假直播，模拟取流请用 FolderSource。"));
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
