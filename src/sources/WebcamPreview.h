#pragma once

#include "sources/WebcamSource.h"

#include <QImage>
#include <QObject>
#include <QThread>

#include <atomic>

// 切到本机摄像头后的无检测预览。不走开线会话：不做 detect / 班次 / DO。
// stop() 同步等到线程退出并释放 VideoCapture，开线才能再打开同一设备。
class WebcamPreview : public QObject
{
    Q_OBJECT

public:
    // 预览用满 WebcamSource 上限，和顶栏开线节拍 liveTargetFps 分开。
    static constexpr int kPreviewFps = 15;

    explicit WebcamPreview(QObject* parent = nullptr);
    ~WebcamPreview() override;

    bool start(int deviceIndex = 0);
    // 同步等到抓帧线程退出。可重入。
    void stop();

    bool isRunning() const { return m_running.load(); }
    bool hasFrame() const { return m_hasFrame.load(); }
    int deviceIndex() const { return m_deviceIndex; }
    int lastWidth() const { return m_lastWidth.load(); }
    int lastHeight() const { return m_lastHeight.load(); }
    QString lastError() const { return m_lastError; }

signals:
    void frameReady(const QImage& image);
    void errorOccurred(const QString& msg);

private:
    void grabLoop();

    WebcamSource m_source;
    QThread* m_thread = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_hasFrame{false};
    std::atomic<int> m_lastWidth{0};
    std::atomic<int> m_lastHeight{0};
    int m_deviceIndex = 0;
    QString m_lastError;
};
