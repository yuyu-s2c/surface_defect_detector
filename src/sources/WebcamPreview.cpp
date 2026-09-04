#include "sources/WebcamPreview.h"
#include "log/AppLog.h"

#include "ImageConvert.h"

WebcamPreview::WebcamPreview(QObject* parent)
    : QObject(parent)
{
}

WebcamPreview::~WebcamPreview()
{
    stop();
}

bool WebcamPreview::start(int deviceIndex)
{
    stop();

    m_deviceIndex = qMax(0, deviceIndex);
    m_lastError.clear();
    m_hasFrame.store(false);
    m_lastWidth.store(0);
    m_lastHeight.store(0);

    m_source.setDeviceIndex(m_deviceIndex);
    m_source.setFps(kPreviewFps);

    connect(&m_source, &IFrameSource::errorOccurred, this, [this](const QString& msg) {
        m_lastError = msg;
        emit errorOccurred(msg);
    });

    if (!m_source.start()) {
        disconnect(&m_source, nullptr, this, nullptr);
        m_lastError = QStringLiteral("无法打开本机摄像头（设备 %1）。"
                                    "检查是否被占用，或 Windows 隐私设置是否允许桌面应用使用相机。")
                          .arg(m_deviceIndex);
        // 不在这里 emit：调用方看返回值，避免和 ViewModel toast 叠两条。
        return false;
    }

    m_running.store(true);
    m_thread = QThread::create([this]() { grabLoop(); });
    m_thread->setObjectName(QStringLiteral("webcam-preview"));
    m_thread->start();
    return true;
}

void WebcamPreview::stop()
{
    if (!m_running.load() && !m_thread)
        return;

    m_source.stop();
    if (m_thread) {
        if (!m_thread->wait(8000)) {
            qCWarning(lcSource) << "WebcamPreview 抓帧线程未在 8s 内退出（read 可能堵在驱动）。不 delete 未结束的 QThread。";
        }
        if (m_thread->isFinished()) {
            delete m_thread;
            m_thread = nullptr;
        }
    }
    disconnect(&m_source, nullptr, this, nullptr);
    m_running.store(false);
}

void WebcamPreview::grabLoop()
{
    CapturedFrame frame;
    while (m_source.grab(frame)) {
        if (frame.bgr.empty())
            continue;
        m_lastWidth.store(frame.bgr.cols);
        m_lastHeight.store(frame.bgr.rows);
        m_hasFrame.store(true);
        emit frameReady(bgrToQImage(frame.bgr));
    }
    m_running.store(false);
}
