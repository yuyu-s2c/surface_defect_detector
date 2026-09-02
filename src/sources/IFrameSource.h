#pragma once

#include <QObject>
#include <QString>

#include <opencv2/core.hpp>

#include <chrono>
#include <cstdint>

// 取流薄接口。本阶段 FolderSource 按 FPS 吐 test/ 图；CameraSource 空实现。
// 真相机来了只换 Source，不改 InspectionSession / ViewModel。
//
// 队列策略（由 InspectionSession 执行，不在 Source 里）：
//   FolderSource：队列满则阻塞取帧，保证跑完一类 test、不丢图
//   CameraSource（P4）：队列满应丢最旧帧，保实时；本阶段 stub 不实现

inline qint64 steadyNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct CapturedFrame
{
    cv::Mat bgr;
    QString path;       // Folder 填文件路径；相机空
    QString defectType; // Folder 从 test/<缺陷>/ 来；相机空
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

signals:
    void errorOccurred(const QString& msg);
};
