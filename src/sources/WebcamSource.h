#pragma once

#include "sources/IFrameSource.h"

#include <QMutex>
#include <QWaitCondition>

#include <opencv2/videoio.hpp>

#include <atomic>

// P4.0 本机 USB 摄像头。不是海康：CameraSource 保持空壳（海康不在本交付范围）。
// overflow 固定丢最旧帧；plannedCount=0（无限流，停线才停）。
class WebcamSource : public IFrameSource
{
public:
    explicit WebcamSource(QObject* parent = nullptr);
    ~WebcamSource() override;

    void setDeviceIndex(int index);
    int deviceIndex() const { return m_index; }
    void setFps(int fps);

    // 自检 / CLI：短开短关，不占用会话。detail 写分辨率或失败原因。
    static bool probe(int index, QString* detail);

    bool start() override;
    void stop() override;
    bool grab(CapturedFrame& out) override;
    bool isRunning() const override;
    int plannedCount() const override;
    QueueOverflowPolicy overflowPolicy() const override;
    int targetFps() const override;

private:
    static bool openCapture(cv::VideoCapture& cap, int index, QString* err);
    static cv::Mat downscaleIfNeeded(const cv::Mat& bgr);

    int m_index = 0;
    int m_fps = 5;
    int m_seq = 0;
    qint64 m_lastGrabNs = 0;
    cv::VideoCapture m_cap;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop{false};

    mutable QMutex m_mutex;
    QWaitCondition m_paceCond;
};
