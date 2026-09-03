#include "InspectionSession.h"

#include "ResultExporter.h"
#include "sources/LogRejectSink.h"

#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QtDebug>

InspectionSession::InspectionSession(DetectionController& ctrl, QObject* parent)
    : QObject(parent)
    , m_ctrl(ctrl)
    , m_reject(std::make_unique<LogRejectSink>())
{
}

InspectionSession::~InspectionSession()
{
    stop();
}

void InspectionSession::setRejectSink(std::unique_ptr<IRejectSink> sink)
{
    if (m_running.load())
        return;
    m_reject = sink ? std::move(sink) : std::make_unique<LogRejectSink>();
}

void InspectionSession::setSessionMeta(const QString& sessionId, const QString& sessionDir,
                                      const QString& engineName, const QString& provider)
{
    if (m_running.load())
        return;
    m_sessionId = sessionId;
    m_sessionDir = sessionDir;
    m_engineName = engineName;
    m_provider = provider;
}

bool InspectionSession::start(std::unique_ptr<IFrameSource> source, const QString& category)
{
    if (m_running.load() || !source || category.isEmpty())
        return false;

    stop();

    m_category = category;
    m_source = std::move(source);
    m_overflow = m_source->overflowPolicy();
    m_targetFps = m_source->targetFps();
    if (m_targetFps < kMinFps)
        m_targetFps = kDefaultFps;
    m_ejectWindowMs = qMax(1, 1000 / m_targetFps);
    m_total = m_source->plannedCount();
    m_done = 0;
    m_ng = 0;
    m_late = 0;
    m_maxQueue = 0;
    m_maxLatencyMs = 0;
    m_dropped.store(0);
    m_queue.clear();
    m_doneNs.clear();
    m_pieces.clear();
    m_lastPieces.clear();
    m_lastSummary = {};
    m_abort.store(false);
    m_sourceFinished.store(false);
    m_stopping.store(false);
    m_startedNs = steadyNowNs();
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
    if (m_overflow == QueueOverflowPolicy::DropOldest) {
        if (m_abort.load())
            return false;
        if (m_queue.size() >= kMaxQueue) {
            m_queue.dequeue();
            m_dropped.fetch_add(1);
        }
        m_queue.enqueue(frame);
        m_frameCond.wakeOne();
        return true;
    }

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
        const bool late = result.detected() && latencyMs > m_ejectWindowMs;

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
        out.lateEject = late;
        out.dropped = m_dropped.load();

        {
            QMutexLocker lock(&m_mutex);
            ++m_done;
            if (result.detected())
                ++m_ng;
            if (late)
                ++m_late;
            m_maxQueue = qMax(m_maxQueue, depth);
            m_maxLatencyMs = qMax(m_maxLatencyMs, latencyMs);
            m_doneNs.push_back(now);
            const qint64 window = 1'000'000'000LL;
            while (!m_doneNs.empty() && now - m_doneNs.front() > window)
                m_doneNs.pop_front();
            out.actualFps = actualFpsLocked();
            out.done = m_done;
            out.lateCount = m_late;

            LivePieceRecord rec;
            rec.category = m_category;
            rec.defectType = frame.defectType;
            rec.path = frame.path;
            rec.detected = result.detected();
            rec.imageScore = result.imageScore;
            rec.imageThreshold = result.imageThreshold;
            rec.latencyMs = latencyMs;
            rec.lateEject = late;
            rec.queueDepth = depth;
            m_pieces.push_back(rec);
        }

        if (result.detected() && m_reject) {
            RejectContext ctx;
            ctx.category = m_category;
            ctx.frame = frame;
            ctx.result = result;
            ctx.latencyMs = latencyMs;
            ctx.lateEject = late;
            m_reject->reject(ctx);
        }

        if (m_abort.load())
            break;

        QPointer<InspectionSession> self(this);
        QMetaObject::invokeMethod(this, [self, out, gen]() {
            if (self && self->m_sessionGen.load() == gen)
                emit self->frameInspected(out);
        }, Qt::QueuedConnection);
    }

    {
        QMutexLocker lock(&m_mutex);
        flushSessionLogLocked();
        m_lastPieces = m_pieces;
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

void InspectionSession::flushSessionLogLocked()
{
    const qint64 now = steadyNowNs();
    LiveSessionSummary s;
    s.sessionId = m_sessionId;
    s.sessionDir = m_sessionDir;
    s.category = m_category;
    s.engineName = m_engineName;
    s.provider = m_provider;
    s.targetFps = m_targetFps;
    s.done = m_done;
    s.planned = m_total;
    s.ok = qMax(0, m_done - m_ng);
    s.ng = m_ng;
    s.dropped = m_dropped.load();
    s.lateEject = m_late;
    s.maxQueue = m_maxQueue;
    s.maxLatencyMs = m_maxLatencyMs;
    s.elapsedSec = (m_startedNs > 0 && now > m_startedNs)
        ? (now - m_startedNs) / 1e9
        : 0.0;
    s.effectiveFps = (s.elapsedSec > 0 && m_done > 0) ? m_done / s.elapsedSec : 0.0;
    m_lastSummary = s;

    if (!m_sessionDir.isEmpty())
        ResultExporter::exportLiveSession(m_sessionDir, s, m_pieces);
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
