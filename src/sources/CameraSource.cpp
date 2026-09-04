#include "sources/CameraSource.h"
#include "log/AppLog.h"

CameraSource::CameraSource(QObject* parent)
    : IFrameSource(parent)
{
}

bool CameraSource::start()
{
    qCWarning(lcSource) << "海康离线（P4 实机）。本机摄像头走 WebcamSource，模拟取流走 FolderSource。";
    emit errorOccurred(QStringLiteral(
        "海康离线（P4 实机）。本机摄像头走 WebcamSource，模拟取流走 FolderSource。"));
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
    // 完整 P4 填海康后 Session 按此丢最旧帧。本阶段 start() 仍失败。
    return QueueOverflowPolicy::DropOldest;
}
