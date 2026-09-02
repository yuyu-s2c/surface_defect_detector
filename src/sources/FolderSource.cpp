#include "sources/FolderSource.h"

#include <opencv2/imgcodecs.hpp>

#include <QDebug>

FolderSource::FolderSource(QObject* parent)
    : IFrameSource(parent)
{
}

void FolderSource::setFromDataset(const DatasetManager& dataset, const QString& category)
{
    m_items.clear();
    m_category = category;
    const QStringList defects = dataset.defectTypes(category);
    for (const QString& defect : defects) {
        const QStringList images = dataset.testImages(category, defect);
        for (const QString& path : images)
            m_items.push_back({path, defect});
    }
}

void FolderSource::setFps(int fps)
{
    m_fps = qBound(1, fps, 15);
}

int FolderSource::fps() const
{
    return m_fps;
}

bool FolderSource::start()
{
    if (m_items.isEmpty())
        return false;
    QMutexLocker lock(&m_mutex);
    m_index = 0;
    m_lastGrabNs = 0;
    m_stop.store(false);
    m_running.store(true);
    return true;
}

void FolderSource::stop()
{
    m_stop.store(true);
    m_running.store(false);
    m_paceCond.wakeAll();
}

bool FolderSource::isRunning() const
{
    return m_running.load();
}

int FolderSource::plannedCount() const
{
    return m_items.size();
}

bool FolderSource::grab(CapturedFrame& out)
{
    while (true) {
        Item item;
        {
            QMutexLocker lock(&m_mutex);
            if (m_stop.load() || m_index >= m_items.size()) {
                m_running.store(false);
                return false;
            }
            // 第一帧立即取；之后按 FPS 节拍。stop() 能唤醒这段 wait。
            if (m_index > 0 && m_fps > 0 && m_lastGrabNs > 0) {
                const qint64 periodNs = 1'000'000'000LL / m_fps;
                while (!m_stop.load()) {
                    const qint64 elapsed = steadyNowNs() - m_lastGrabNs;
                    if (elapsed >= periodNs)
                        break;
                    const qint64 remainMs = (periodNs - elapsed) / 1'000'000;
                    m_paceCond.wait(&m_mutex, static_cast<unsigned long>(qMax<qint64>(1, remainMs)));
                }
                if (m_stop.load()) {
                    m_running.store(false);
                    return false;
                }
            }
            item = m_items[m_index];
            ++m_index;
        }

        cv::Mat img = cv::imread(item.path.toLocal8Bit().constData(), cv::IMREAD_COLOR);
        if (img.empty()) {
            qWarning() << "FolderSource 跳过无法读取的图：" << item.path;
            continue;
        }

        out.bgr = img;
        out.path = item.path;
        out.defectType = item.defectType;
        out.grabbedNs = steadyNowNs();
        m_lastGrabNs = out.grabbedNs;
        return true;
    }
}
