#pragma once

#include "DetectionController.h"
#include "IDetectionEngine.h"
#include "sources/IFrameSource.h"
#include "sources/LogRejectSink.h"

#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

#include <atomic>
#include <deque>
#include <memory>

// 取流会话：Source 按 FPS 入队，检测线程同步 ctrl.detect()，NG 走 IRejectSink。
// 不走 DetectionController::*Async（忙碌时会丢帧，和「跑完一类 test」冲突）。
struct LiveInspectedFrame
{
    QString category;
    QString path;
    QString defectType;
    cv::Mat bgr;
    DetectionResult result;
    int queueDepth = 0;
    int queueMax = 0;
    qint64 latencyMs = 0;
    double actualFps = 0.0;
    int done = 0;
    int total = 0;
};

class InspectionSession : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxQueue = 8;
    static constexpr int kMinFps = 1;
    static constexpr int kMaxFps = 15;
    static constexpr int kDefaultFps = 5;

    explicit InspectionSession(DetectionController& ctrl, QObject* parent = nullptr);
    ~InspectionSession() override;

    bool isRunning() const { return m_running.load(); }
    int queueMax() const { return kMaxQueue; }

    // source 所有权转给 session。调用方须已 prepareEngine。
    bool start(std::unique_ptr<IFrameSource> source, const QString& category);
    // 同步等到两线程退出，不泄漏。可重入。
    void stop();

signals:
    void frameInspected(const LiveInspectedFrame& frame);
    void finished(int total, int ngCount);
    void errorOccurred(const QString& msg);
    void runningChanged();

private:
    bool enqueueFrame(const CapturedFrame& frame);
    bool dequeueFrame(CapturedFrame& frame, int* depthAfter);
    void grabLoop();
    void detectLoop(quint64 gen);
    void cleanupThreads();
    double actualFpsLocked() const;

    DetectionController& m_ctrl;
    LogRejectSink m_reject;
    std::unique_ptr<IFrameSource> m_source;

    QThread* m_grabThread = nullptr;
    QThread* m_detectThread = nullptr;

    QQueue<CapturedFrame> m_queue;
    std::deque<qint64> m_doneNs;
    mutable QMutex m_mutex;
    QWaitCondition m_frameCond;
    QWaitCondition m_spaceCond;

    QString m_category;
    int m_total = 0;
    int m_done = 0;
    int m_ng = 0;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_abort{false};
    std::atomic<bool> m_sourceFinished{false};
    std::atomic<bool> m_stopping{false};
    std::atomic<quint64> m_sessionGen{0};
};
