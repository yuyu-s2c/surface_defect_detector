#include "sources/WebcamSource.h"
#include "log/AppLog.h"

#include <opencv2/imgproc.hpp>

#include <QtGlobal>

namespace {
constexpr int kMaxLongSide = 700;
}

WebcamSource::WebcamSource(QObject* parent)
    : IFrameSource(parent)
{
}

WebcamSource::~WebcamSource()
{
    stop();
    m_cap.release();
}

void WebcamSource::setDeviceIndex(int index)
{
    m_index = qMax(0, index);
}

void WebcamSource::setFps(int fps)
{
    m_fps = qBound(1, fps, 15);
}

bool WebcamSource::openCapture(cv::VideoCapture& cap, int index, QString* err)
{
    cap.release();
#ifdef _WIN32
    // 先 DSHOW（MinGW 上更常见），失败再 MSMF（Win11 隐私授权后有时只认这条）。
    const bool opened = cap.open(index, cv::CAP_DSHOW) || cap.open(index, cv::CAP_MSMF);
    if (!opened) {
        if (err) {
            *err = QStringLiteral("无法打开本机摄像头（设备 %1，DirectShow/MSMF）。"
                                  "检查是否被占用，或 Windows 隐私设置是否允许桌面应用使用相机。")
                       .arg(index);
        }
        return false;
    }
#else
    if (!cap.open(index)) {
        if (err) {
            *err = QStringLiteral("无法打开本机摄像头（设备 %1）。").arg(index);
        }
        return false;
    }
#endif
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
    return true;
}

cv::Mat WebcamSource::downscaleIfNeeded(const cv::Mat& bgr)
{
    if (bgr.empty())
        return {};
    const int longSide = qMax(bgr.cols, bgr.rows);
    if (longSide <= kMaxLongSide)
        return bgr;
    // EfficientAD 热图会上采样回原图，1080p 连通域会拖检测线程、人为打满队列。
    const double scale = double(kMaxLongSide) / double(longSide);
    cv::Mat small;
    cv::resize(bgr, small, cv::Size(), scale, scale, cv::INTER_AREA);
    return small;
}

bool WebcamSource::probe(int index, QString* detail)
{
    cv::VideoCapture cap;
    QString err;
    if (!openCapture(cap, qMax(0, index), &err)) {
        if (detail)
            *detail = err;
        return false;
    }
    cv::Mat frame;
    const bool ok = cap.read(frame) && !frame.empty();
    const int w = ok ? frame.cols : 0;
    const int h = ok ? frame.rows : 0;
    cap.release();
    if (!ok) {
        if (detail) {
            *detail = QStringLiteral("已打开设备 %1，但读不到帧。")
                          .arg(qMax(0, index));
        }
        return false;
    }
    if (detail) {
        *detail = QStringLiteral("本机摄像头 · OpenCV · 设备 %1 · %2×%3")
                      .arg(qMax(0, index))
                      .arg(w)
                      .arg(h);
    }
    return true;
}

bool WebcamSource::start()
{
    QString err;
    if (!openCapture(m_cap, m_index, &err)) {
        // 不在这里 emit：InspectionSession::start 已连接 errorOccurred，
        // 再发一次会和 ViewModel 的「启动失败」叠两条。调用方看返回值即可。
        qCWarning(lcSource) << err;
        return false;
    }
    QMutexLocker lock(&m_mutex);
    m_seq = 0;
    m_lastGrabNs = 0;
    m_stop.store(false);
    m_running.store(true);
    return true;
}

void WebcamSource::stop()
{
    m_stop.store(true);
    m_running.store(false);
    m_paceCond.wakeAll();
    // read() 可能堵在驱动里；release 让 grab 线程尽快返回。
    m_cap.release();
}

bool WebcamSource::isRunning() const
{
    return m_running.load();
}

int WebcamSource::plannedCount() const
{
    return 0;
}

QueueOverflowPolicy WebcamSource::overflowPolicy() const
{
    return QueueOverflowPolicy::DropOldest;
}

int WebcamSource::targetFps() const
{
    return m_fps;
}

bool WebcamSource::grab(CapturedFrame& out)
{
    while (true) {
        {
            QMutexLocker lock(&m_mutex);
            if (m_stop.load()) {
                m_running.store(false);
                return false;
            }
            if (m_seq > 0 && m_fps > 0 && m_lastGrabNs > 0) {
                const qint64 periodNs = 1'000'000'000LL / m_fps;
                while (!m_stop.load()) {
                    const qint64 elapsed = steadyNowNs() - m_lastGrabNs;
                    if (elapsed >= periodNs)
                        break;
                    const qint64 remainMs = (periodNs - elapsed) / 1'000'000;
                    m_paceCond.wait(&m_mutex,
                                    static_cast<unsigned long>(qMax<qint64>(1, remainMs)));
                }
                if (m_stop.load()) {
                    m_running.store(false);
                    return false;
                }
            }
        }

        cv::Mat raw;
        if (!m_cap.isOpened() || !m_cap.read(raw) || raw.empty()) {
            if (m_stop.load()) {
                m_running.store(false);
                return false;
            }
            qCWarning(lcSource) << "WebcamSource 读帧失败，设备" << m_index;
            emit errorOccurred(QStringLiteral("本机摄像头读帧失败（设备 %1）。").arg(m_index));
            m_running.store(false);
            return false;
        }

        cv::Mat bgr = downscaleIfNeeded(raw);
        if (bgr.empty())
            continue;

        ++m_seq;
        out.bgr = bgr;
        // 合成名给 FileRejectSink 叠图用，避免空 path 撞成同一份 _ann.png。
        out.path = QStringLiteral("cam_%1").arg(m_seq, 6, 10, QLatin1Char('0'));
        out.defectType.clear();
        out.grabbedNs = steadyNowNs();
        m_lastGrabNs = out.grabbedNs;
        return true;
    }
}
