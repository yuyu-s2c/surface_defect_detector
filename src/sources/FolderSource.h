#pragma once

#include "DatasetManager.h"
#include "sources/IFrameSource.h"

#include <QMutex>
#include <QString>
#include <QVector>
#include <QWaitCondition>

#include <atomic>

// 按设定 FPS 依次吐出某类别 test/ 全部图（含 good），顺序与 runBatch 一致。
// 这是本工位唯一直播路径（相机离线）。不循环；grab() 在 EOF 返回 false。
class FolderSource : public IFrameSource
{
public:
    explicit FolderSource(QObject* parent = nullptr);

    void setFromDataset(const DatasetManager& dataset, const QString& category);
    void setFps(int fps);
    int fps() const;
    void setOverflowPolicy(QueueOverflowPolicy policy);

    bool start() override;
    void stop() override;
    bool grab(CapturedFrame& out) override;
    bool isRunning() const override;
    int plannedCount() const override;
    QueueOverflowPolicy overflowPolicy() const override;
    int targetFps() const override;

private:
    struct Item
    {
        QString path;
        QString defectType;
    };

    QVector<Item> m_items;
    QString m_category;
    int m_fps = 5;
    QueueOverflowPolicy m_overflow = QueueOverflowPolicy::Block;
    int m_index = 0;
    qint64 m_lastGrabNs = 0;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop{false};

    mutable QMutex m_mutex;
    QWaitCondition m_paceCond;
};
