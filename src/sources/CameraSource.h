#pragma once

#include "sources/IFrameSource.h"

// 完整 Phase 4 填海康 MVS/MVD。本阶段空实现：start() 失败「海康离线」。
// 禁止当假直播；本机 USB 走 WebcamSource，回归走 FolderSource。
class CameraSource : public IFrameSource
{
public:
    explicit CameraSource(QObject* parent = nullptr);

    bool start() override;
    void stop() override;
    bool grab(CapturedFrame& out) override;
    bool isRunning() const override;
    QueueOverflowPolicy overflowPolicy() const override;
};
