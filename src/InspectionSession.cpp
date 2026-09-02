#include "InspectionSession.h"

#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QtDebug>

InspectionSession::InspectionSession(DetectionController& ctrl, QObject* parent)
    : QObject(parent)
    , m_ctrl(ctrl)
{
}

InspectionSession::~InspectionSession()
{
    stop();
}

bool InspectionSession::start(std::unique_ptr<IFrameSource> source, const QString& category)
{
    if (m_running.load() || !source || category.isEmpty())
        return false;

    stop();

    m_category = category;
    m_source = std::move(source);
    m_total = m_source->plannedCount();
    m_done = 0;
    m_ng = 0;
    m_queue.clear();
    m_doneNs.clear();
    m_abort.store(false);
    m_sourceFinished.store(false);
    m_stopping.store(false);
    const quint64 gen = ++m_sessionGen;

    connect(m_source.get(), &IFrameSource::errorOccurred,
            this, &InspectionSession::errorOccurred);

    if (!m_source->start()) {
        m_source.reset();
        return false;
    }

    m_running.store(true);
    emit runningChanged();

    m_detectThread = QThread::create([this, gen]() { detectLoop(gen); });
    m_detectThread->setObjectName(QStringLiteral("inspect-detect"));
    m_grabThread = QThread::create([this]() { grabLoop(); });
    m_grabThread->setObjectName(QStringLiteral("inspect-source"));

    m_detectThread->start();
    m_grabThread->start();
    return true;
}

void InspectionSession::stop()
{
    if (!m_running.load() && !m_grabThread && !m_detectThread)
        return;

    m_stopping.store(true);
    ++m_sessionGen;
    m_abort.store(true);
    if (m_source)
        m_source->stop();
    m_frameCond.wakeAll();
    m_spaceCond.wakeAll();

    if (m_grabThread) {
        m_grabThread->wait(8000);
    }
    if (m_detectThread) {
        // 等当前这张 detect 结束；DML 单张通常 <200ms，给足余量
        m_detectThread->wait(20000);
    }

    cleanupThreads();
    if (m_running.exchange(false))
        emit runningChanged();
    m_stopping.store(false);
}

void InspectionSession::cleanupThreads()
{
    if (m_grabThread) {
        m_grabThread->wait();
        delete m_grabThread;
        m_grabThread = nullptr;
    }
    if (m_detectThread) {
        m_detectThread->wait();
        delete m_detectThread;
        m_detectThread = nullptr;
    }
    m_source.reset();
    QMutexLocker lock(&m_mutex);
    m_queue.clear();
}

bool InspectionSession::enqueueFrame(const CapturedFrame& frame)
{
    QMutexLocker lock(&m_mutex);
    while (m_queue.size() >= kMaxQueue && !m_abort.load())
        m_spaceCond.wait(&m_mutex);
    if (m_abort.load())
        return false;
    m_queue.enqueue(frame);
    m_frameCond.wakeOne();
    return true;
}

bool InspectionSession::dequeueFrame(CapturedFrame& frame, int* depthAfter)
{
    QMutexLocker lock(&m_mutex);
    while (m_queue.isEmpty() && !m_abort.load() && !m_sourceFinished.load())
        m_frameCond.wait(&m_mutex);
    if (m_abort.load())
        return false;
    if (m_queue.isEmpty())
        return false;
    frame = m_queue.dequeue();
    if (depthAfter)
        *depthAfter = m_queue.size();
    m_spaceCond.wakeOne();
    return true;
}

void InspectionSession::grabLoop()
{
    IFrameSource* source = m_source.get();
    while (!m_abort.load() && source) {
        CapturedFrame frame;
        if (!source->grab(frame))
            break;
        if (!enqueueFrame(frame))
            break;
    }
    m_sourceFinished.store(true);
    m_frameCond.wakeAll();
}

void InspectionSession::detectLoop(quint64 gen)
{
    while (true) {
        CapturedFrame frame;
        int depth = 0;
        if (!dequeueFrame(frame, &depth))
            break;

        const DetectionResult result = m_ctrl.detect(m_category, frame.bgr);
        const qint64 now = steadyNowNs();
        const qint64 latencyMs = (frame.grabbedNs > 0)
            ? qMax<qint64>(0, (now - frame.grabbedNs) / 1'000'000)
            : 0;

        LiveInspectedFrame out;
        out.category = m_category;
        out.path = frame.path;
        out.defectType = frame.defectType;
        out.bgr = frame.bgr;
        out.result = result;
        out.queueDepth = depth;
        out.queueMax = kMaxQueue;
        out.latencyMs = latencyMs;
        out.total = m_total;

        {
            QMutexLocker lock(&m_mutex);
            ++m_done;
            if (result.detected())
                ++m_ng;
            m_doneNs.push_back(now);
            const qint64 window = 1'000'000'000LL;
            while (!m_doneNs.empty() && now - m_doneNs.front() > window)
                m_doneNs.pop_front();
            out.actualFps = actualFpsLocked();
            out.done = m_done;
        }

        if (result.detected())
            m_reject.reject(m_category, frame, result);

        if (m_abort.load())
            break;

        QPointer<InspectionSession> self(this);
        QMetaObject::invokeMethod(this, [self, out, gen]() {
            if (self && self->m_sessionGen.load() == gen)
                emit self->frameInspected(out);
        }, Qt::QueuedConnection);
    }

    if (!m_abort.load() && !m_stopping.load()) {
        const int done = m_done;
        const int ng = m_ng;
        QPointer<InspectionSession> self(this);
        QMetaObject::invokeMethod(this, [self, done, ng, gen]() {
            if (!self || self->m_sessionGen.load() != gen)
                return;
            self->cleanupThreads();
            if (self->m_running.exchange(false))
                emit self->runningChanged();
            emit self->finished(done, ng);
        }, Qt::QueuedConnection);
    }
}

double InspectionSession::actualFpsLocked() const
{
    if (m_doneNs.size() >= 2) {
        const qint64 span = m_doneNs.back() - m_doneNs.front();
        if (span <= 0)
            return 0.0;
        return (m_doneNs.size() - 1) * 1e9 / double(span);
    }
    if (m_doneNs.size() == 1)
        return 1.0;
    return 0.0;
}
