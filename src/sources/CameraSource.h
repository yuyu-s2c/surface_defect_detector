#pragma once

#include "sources/IFrameSource.h"

// 海康不在本交付范围。空实现：start() 失败「海康离线」。
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
