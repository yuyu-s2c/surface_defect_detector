#pragma once

#include "sources/IFrameSource.h"

// Phase 4 填海康 MVS/MVD。本阶段空实现：start() 失败「相机离线」。
// 禁止当假直播；GUI / --live-smoke 走 FolderSource。
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
