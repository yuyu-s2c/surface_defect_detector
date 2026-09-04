#pragma once

#include <QObject>
#include <QString>

#include <opencv2/core.hpp>

#include <chrono>
#include <cstdint>

// 取流薄接口。FolderSource 按 FPS 吐 test/ 图；WebcamSource 本机摄像头（P4.0）；
// CameraSource 空实现（完整 P4 填海康）。真工业相机来了只换 CameraSource。
//
// 队列策略（由 InspectionSession 执行，不在 Source 里）：
//   Block：队列满则阻塞取帧（Folder 默认，保证跑完一类 test、不丢图）
//   DropOldest：队列满丢队头，保实时（Webcam / 海康；--live-smoke --overflow drop 用来测）

inline qint64 steadyNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

enum class QueueOverflowPolicy {
    Block,
    DropOldest
};

struct CapturedFrame
{
    cv::Mat bgr;
    QString path;       // Folder 填文件路径；Webcam 填 cam_000123；海康空
    QString defectType; // Folder 从 test/<缺陷>/ 来；Webcam / 海康空
    qint64 grabbedNs = 0;
};

class IFrameSource : public QObject
{
    Q_OBJECT

public:
    explicit IFrameSource(QObject* parent = nullptr) : QObject(parent) {}
    ~IFrameSource() override = default;

    // 打开源。失败发 errorOccurred，返回 false。调用线程：GUI，线程启动前。
    virtual bool start() = 0;
    // 必须能从其他线程唤醒阻塞中的 grab()。
    virtual void stop() = 0;
    // 阻塞到下一帧；EOF / stop 返回 false。只在取流线程调用。
    virtual bool grab(CapturedFrame& out) = 0;
    virtual bool isRunning() const = 0;
    // Folder 返回 playlist 长度；相机 0
    virtual int plannedCount() const { return 0; }
    virtual QueueOverflowPolicy overflowPolicy() const { return QueueOverflowPolicy::Block; }
    virtual int targetFps() const { return 0; }

signals:
    void errorOccurred(const QString& msg);
};
