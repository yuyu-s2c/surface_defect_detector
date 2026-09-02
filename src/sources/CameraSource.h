#pragma once

#include "sources/IFrameSource.h"

// Phase 4 填海康 MVS/MVD。本阶段空实现：start() 失败，不接假 SDK、不发假报文。
class CameraSource : public IFrameSource
{
public:
    explicit CameraSource(QObject* parent = nullptr);

    bool start() override;
    void stop() override;
    bool grab(CapturedFrame& out) override;
    bool isRunning() const override;
};
