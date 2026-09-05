#include "DetectionController.h"

#include "DetectionEngine.h"
#include "DLDetectionEngine.h"

#include <opencv2/imgcodecs.hpp>

#include <QFileInfo>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include "log/AppLog.h"

namespace {

void applyTraditionalParams(DetectionEngine* engine, const TraditionalParams& p)
{
    const TraditionalParams s = p.sanitized();
    engine->zAggThreshold = s.zAggThreshold;
    engine->morphCloseKernel = s.morphCloseKernel;
    engine->minDefectArea = s.minDefectArea;
    engine->imageLevelMinArea = s.imageLevelMinArea;
}

void applyDLParams(DLDetectionEngine* engine, const DLParams& p)
{
    const DLParams s = p.sanitized();
    engine->thresholdSigma = s.thresholdSigma;
    engine->pixelSigma = s.pixelSigma;
    engine->morphCloseKernel = s.morphCloseKernel;
    engine->minDefectArea = s.minDefectArea;
    engine->imageLevelMinArea = s.imageLevelMinArea;
    engine->topK = s.topK;
    engine->roiRadiusRatio = s.roiRadiusRatio;
}


// anomalib Engine.export 落点，其次扁平回退。新类接入只放文件，不改这里的拼接规则。
QString onnxExportedPath(const QString& root, const QString& category)
{
    return QStringLiteral("%1/models/%2/weights/onnx/%2.onnx").arg(root, category);
}

QString onnxFallbackPath(const QString& root, const QString& category)
{
    return QStringLiteral("%1/models/%2/%2.onnx").arg(root, category);
}

QString resolveOnnxModelPath(const QString& root, const QString& category)
{
    const QString exported = onnxExportedPath(root, category);
    if (QFileInfo::exists(exported))
        return exported;
    const QString fallback = onnxFallbackPath(root, category);
    if (QFileInfo::exists(fallback))
        return fallback;
    return {};
}

} // namespace

DetectionController::DetectionController(QObject* parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(1);
}

DetectionController::~DetectionController()
{
    m_abort.store(true);
    m_pool.waitForDone();
    qDeleteAll(m_cvEngines);
    qDeleteAll(m_dlEngines);
}

QMap<QString, IDetectionEngine*>& DetectionController::engineMap()
{
    return m_engineKind == EngineKind::DL ? m_dlEngines : m_cvEngines;
}

const QMap<QString, IDetectionEngine*>& DetectionController::engineMap() const
{
    return m_engineKind == EngineKind::DL ? m_dlEngines : m_cvEngines;
}

QMap<QString, BatchMetrics>& DetectionController::lastBatchMap(EngineKind kind)
{
    return kind == EngineKind::DL ? m_lastDl : m_lastCv;
}

const QMap<QString, BatchMetrics>& DetectionController::lastBatchMap(EngineKind kind) const
{
    return kind == EngineKind::DL ? m_lastDl : m_lastCv;
}

bool DetectionController::isBusy() const
{
    QMutexLocker lock(&m_mutex);
    return m_busy;
}

EngineKind DetectionController::engineKind() const
{
    QMutexLocker lock(&m_mutex);
    return m_engineKind;
}

void DetectionController::attachProgress(IDetectionEngine* engine, EngineKind kind,
                                         const QString& category)
{
    engine->setProgressCallback([this, kind, category](int current, int total) -> bool {
        if (m_abort.load())
            return false;
        const QString msg = (kind == EngineKind::DL)
            ? QStringLiteral("标定阈值（%1）%2/%3 — 扫描 train/good，不是训练")
                  .arg(category).arg(current).arg(total)
            : QStringLiteral("构建传统参考模型（%1）%2/%3")
                  .arg(category).arg(current).arg(total);
        emit progressChanged(current, total, msg);
        return true;
    });
}

void DetectionController::startJob(std::function<void()> fn)
{
    m_pool.start(QRunnable::create([fn = std::move(fn)]() { fn(); }));
}

bool DetectionController::loadDataset(const QString& rootPath)
{
    if (m_dataset.scan(rootPath) == 0)
        return false;
    QMutexLocker lock(&m_mutex);
    // 数据集换了，已缓存的引擎（基于旧良品图构建）与批量结果全部失效
    qDeleteAll(m_cvEngines);
    qDeleteAll(m_dlEngines);
    m_cvEngines.clear();
    m_dlEngines.clear();
    m_lastCv.clear();
    m_lastDl.clear();
    m_traditionalParams.clear();
    m_dlParams.clear();
    return true;
}

void DetectionController::setEngineKind(EngineKind kind)
{
    QMutexLocker lock(&m_mutex);
    m_engineKind = kind;
}

void DetectionController::setOrtEpKind(OrtEpKind kind)
{
    QMutexLocker lock(&m_mutex);
    if (m_ortEpKind == kind)
        return;
    m_ortEpKind = kind;
    qDeleteAll(m_dlEngines);
    m_dlEngines.clear();
    m_lastDl.clear();
}

OrtEpKind DetectionController::ortEpKind() const
{
    QMutexLocker lock(&m_mutex);
    return m_ortEpKind;
}

QString DetectionController::dlProviderLabel(const QString& category) const
{
    QMutexLocker lock(&m_mutex);
    auto* engine = dynamic_cast<DLDetectionEngine*>(m_dlEngines.value(category, nullptr));
    return engine ? engine->activeProvider() : QString();
}

QString DetectionController::dlModelPath(const QString& category) const
{
    QMutexLocker lock(&m_mutex);
    auto* engine = dynamic_cast<DLDetectionEngine*>(m_dlEngines.value(category, nullptr));
    if (engine)
        return engine->modelPath();
    const QString root = m_dataset.rootPath();
    lock.unlock();
    return resolveOnnxModelPath(root, category);
}

QStringList DetectionController::onnxModelCandidates(const QString& category) const
{
    const QString root = m_dataset.rootPath();
    return {onnxExportedPath(root, category), onnxFallbackPath(root, category)};
}

bool DetectionController::hasOnnxModel(const QString& category) const
{
    return !resolveOnnxModelPath(m_dataset.rootPath(), category).isEmpty();
}

bool DetectionController::hasCalibCache(const QString& category) const
{
    const QString model = resolveOnnxModelPath(m_dataset.rootPath(), category);
    if (model.isEmpty())
        return false;
    return QFileInfo::exists(DLDetectionEngine::calibCachePathFor(model));
}

bool DetectionController::dlLoadedCalibFromCache(const QString& category) const
{
    QMutexLocker lock(&m_mutex);
    auto* engine = dynamic_cast<DLDetectionEngine*>(m_dlEngines.value(category, nullptr));
    return engine && engine->loadedCalibFromCache();
}

int DetectionController::trainGoodCount(const QString& category) const
{
    return m_dataset.trainGoodImages(category).size();
}

QString DetectionController::ortEpPolicyLabel() const
{
    switch (ortEpKind()) {
    case OrtEpKind::Cpu:
        return QStringLiteral("CPU");
    case OrtEpKind::Dml:
        return QStringLiteral("DirectML（强制）");
    case OrtEpKind::Auto:
    default:
        return QStringLiteral("自动（DirectML，失败回 CPU）");
    }
}

void DetectionController::setTraditionalParams(const QString& category,
                                               const TraditionalParams& p)
{
    QMutexLocker lock(&m_mutex);
    m_traditionalParams.insert(category, p);
    auto* engine = dynamic_cast<DetectionEngine*>(m_cvEngines.value(category, nullptr));
    if (engine)
        applyTraditionalParams(engine, p);
}

TraditionalParams DetectionController::traditionalParams(const QString& category) const
{
    QMutexLocker lock(&m_mutex);
    if (m_traditionalParams.contains(category))
        return m_traditionalParams.value(category);
    return TraditionalParams::defaults();
}

void DetectionController::setDLParams(const QString& category, const DLParams& p)
{
    QMutexLocker lock(&m_mutex);
    m_dlParams.insert(category, p);
    auto* engine = dynamic_cast<DLDetectionEngine*>(m_dlEngines.value(category, nullptr));
    if (engine)
        applyDLParams(engine, p);
}

DLParams DetectionController::dlParams(const QString& category) const
{
    QMutexLocker lock(&m_mutex);
    if (m_dlParams.contains(category))
        return m_dlParams.value(category);
    return DLParams::defaultsFor(category);
}

IDetectionEngine* DetectionController::engineFor(const QString& category)
{
    QMutexLocker lock(&m_mutex);
    auto& cached = engineMap();
    if (IDetectionEngine* engine = cached.value(category, nullptr))
        return engine;
    const EngineKind kind = m_engineKind;
    const OrtEpKind epKind = m_ortEpKind;
    const TraditionalParams cvParams = m_traditionalParams.contains(category)
        ? m_traditionalParams.value(category)
        : TraditionalParams::defaults();
    const DLParams dlP = m_dlParams.contains(category)
        ? m_dlParams.value(category)
        : DLParams::defaultsFor(category);
    const QString root = m_dataset.rootPath();
    const QStringList good = m_dataset.trainGoodImages(category);
    lock.unlock();

    IDetectionEngine* newEngine = nullptr;
    if (kind == EngineKind::DL) {
        const QString modelPath = resolveOnnxModelPath(root, category);
        if (modelPath.isEmpty()) {
            qCWarning(lcEngine) << "DL 模型不存在，试过:"
                       << onnxExportedPath(root, category)
                       << "和" << onnxFallbackPath(root, category);
            return nullptr;
        }
        auto* dl = new DLDetectionEngine(modelPath, epKind);
        applyDLParams(dl, dlP);
        newEngine = dl;
        emit progressChanged(0, 0, QStringLiteral("正在加载 ONNX 模型（%1）…").arg(category));
    } else {
        auto* cv = new DetectionEngine;
        applyTraditionalParams(cv, cvParams);
        newEngine = cv;
    }
    attachProgress(newEngine, kind, category);
    if (!newEngine->buildReference(good)) {
        delete newEngine;
        return nullptr;
    }
    if (auto* dl = dynamic_cast<DLDetectionEngine*>(newEngine)) {
        const QString msg = dl->loadedCalibFromCache()
            ? QStringLiteral("已加载 %1（%2，复用标定缓存）")
                  .arg(category, dl->activeProvider())
            : QStringLiteral("已加载 %1（%2，完成阈值标定）")
                  .arg(category, dl->activeProvider());
        emit progressChanged(0, 0, msg);
    }

    lock.relock();
    if (m_engineKind != kind) {
        delete newEngine;
        lock.unlock();
        return engineFor(category);
    }
    auto& map = engineMap();
    if (IDetectionEngine* existing = map.value(category, nullptr)) {
        delete newEngine;
        return existing;
    }
    map.insert(category, newEngine);
    return newEngine;
}

bool DetectionController::prepareEngine(const QString& category)
{
    return engineFor(category) != nullptr;
}

DetectionResult DetectionController::detect(const QString& category, const cv::Mat& image)
{
    IDetectionEngine* engine = engineFor(category);
    if (!engine)
        return {};
    return engine->detect(image);
}

bool DetectionController::runBatch(const QString& category, BatchMetrics& out)
{
    IDetectionEngine* engine = engineFor(category);
    if (!engine)
        return false;

    out = BatchMetrics{};
    const QStringList defects = m_dataset.defectTypes(category);
    int total = 0;
    for (const QString& defect : defects)
        total += m_dataset.testImages(category, defect).size();
    int done = 0;

    for (const QString& defect : defects) {
        const bool isDefect = (defect != QStringLiteral("good"));
        const QStringList images = m_dataset.testImages(category, defect);
        for (const QString& imgPath : images) {
            if (m_abort.load())
                return false;
            cv::Mat img = cv::imread(imgPath.toLocal8Bit().constData(), cv::IMREAD_COLOR);
            if (img.empty())
                continue;
            const DetectionResult r = engine->detect(img);
            const QString gtPath = m_dataset.groundTruthMask(category, defect, imgPath);
            const PixelMetrics pm = ResultEvaluator::evaluatePixel(r.defectMask, gtPath);
            out.pixel[defect] += pm;
            out.image[defect] += ResultEvaluator::evaluateImage(r.detected(), isDefect);
            out.imageByArea[defect] += ResultEvaluator::evaluateImage(r.detectedByArea(), isDefect);

            BatchImageRecord rec;
            rec.defectType = defect;
            rec.imagePath = imgPath;
            rec.result = r;
            rec.pixel = pm;
            out.records.push_back(rec);

            ++done;
            emit progressChanged(done, total,
                                 QStringLiteral("检测 %1 / %2 / %3（%4/%5）")
                                     .arg(category, defect, QFileInfo(imgPath).fileName())
                                     .arg(done).arg(total));
            emit imageProcessed(defect, imgPath, r, pm);
        }
    }
    QMutexLocker lock(&m_mutex);
    lastBatchMap(m_engineKind).insert(category, out);
    return true;
}

bool DetectionController::runBatchWithEngine(const QString& category, EngineKind kind,
                                             BatchMetrics& out)
{
    const EngineKind prev = engineKind();
    setEngineKind(kind);
    const bool ok = runBatch(category, out);
    setEngineKind(prev);
    return ok;
}

const BatchMetrics* DetectionController::lastBatch(EngineKind kind, const QString& category) const
{
    QMutexLocker lock(&m_mutex);
    const auto& map = lastBatchMap(kind);
    auto it = map.constFind(category);
    if (it == map.constEnd())
        return nullptr;
    return &it.value();
}

void DetectionController::prepareEngineAsync(const QString& category)
{
    QMutexLocker lock(&m_mutex);
    if (m_busy)
        return;
    m_busy = true;
    lock.unlock();
    emit busyChanged(true);
    emit progressChanged(0, 0, QStringLiteral("准备取流引擎（必要时先标定阈值，不是训练）…"));
    startJob([this, category]() {
        const bool ok = !m_abort.load() && prepareEngine(category);
        if (m_abort.load())
            return;
        QPointer<DetectionController> self(this);
        QMetaObject::invokeMethod(this, [self, ok, category]() {
            if (!self)
                return;
            // 先清 busy，再通知取流启动，避免 finishJobOnGui 把 pending 单张检和 live detect 叠上
            self->finishJobOnGui();
            emit self->enginePrepared(ok, category);
        }, Qt::QueuedConnection);
    });
}

void DetectionController::prepareAndDetectAsync(const QString& category, const cv::Mat& image)
{
    const cv::Mat clone = image.clone();
    QMutexLocker lock(&m_mutex);
    ++m_detectGen;
    const quint64 gen = m_detectGen;
    if (m_busy) {
        m_pendingDetect = PendingDetect{category, clone, gen};
        return;
    }
    m_busy = true;
    lock.unlock();
    emit busyChanged(true);
    startJob([this, category, clone, gen]() { runDetectJob(category, clone, gen); });
}

void DetectionController::runDetectJob(const QString& category, cv::Mat image, quint64 gen)
{
    emit progressChanged(0, 0, QStringLiteral("准备检测引擎…"));
    IDetectionEngine* engine = engineFor(category);
    DetectionResult result;
    const bool ok = engine != nullptr;
    if (ok) {
        emit progressChanged(0, 0, QStringLiteral("正在推理当前图…"));
        result = engine->detect(image);
    }
    if (m_abort.load())
        return;
    QPointer<DetectionController> self(this);
    QMetaObject::invokeMethod(this, [self, ok, result, gen]() {
        if (!self)
            return;
        bool stale = false;
        {
            QMutexLocker lock(&self->m_mutex);
            stale = (gen != self->m_detectGen);
        }
        if (!stale)
            emit self->currentDetectFinished(ok, result);
        self->finishJobOnGui();
    }, Qt::QueuedConnection);
}

void DetectionController::runBatchAsync(const QString& category)
{
    QMutexLocker lock(&m_mutex);
    if (m_busy)
        return;
    m_busy = true;
    lock.unlock();
    emit busyChanged(true);
    startJob([this, category]() {
        BatchMetrics metrics;
        const bool ok = !m_abort.load() && runBatch(category, metrics);
        if (m_abort.load())
            return;
        QPointer<DetectionController> self(this);
        QMetaObject::invokeMethod(this, [self, ok, category]() {
            if (!self)
                return;
            emit self->batchFinished(ok, category);
            self->finishJobOnGui();
        }, Qt::QueuedConnection);
    });
}

void DetectionController::compareAsync(const QString& category)
{
    QMutexLocker lock(&m_mutex);
    if (m_busy)
        return;
    m_busy = true;
    lock.unlock();
    emit busyChanged(true);
    startJob([this, category]() {
        bool ok = true;
        const EngineKind prev = engineKind();
        const bool hasCv = lastBatch(EngineKind::Traditional, category) != nullptr;
        const bool hasDl = lastBatch(EngineKind::DL, category) != nullptr;
        if (!hasCv) {
            emit progressChanged(0, 0, QStringLiteral("对比：运行传统 CV 批量…"));
            BatchMetrics metrics;
            ok = runBatchWithEngine(category, EngineKind::Traditional, metrics);
        }
        if (ok && !hasDl) {
            emit progressChanged(0, 0, QStringLiteral("对比：运行 DL 批量…"));
            BatchMetrics metrics;
            ok = runBatchWithEngine(category, EngineKind::DL, metrics);
        }
        setEngineKind(prev);
        if (m_abort.load())
            return;
        QPointer<DetectionController> self(this);
        QMetaObject::invokeMethod(this, [self, ok, category]() {
            if (!self)
                return;
            emit self->compareFinished(ok, category);
            self->finishJobOnGui();
        }, Qt::QueuedConnection);
    });
}

void DetectionController::finishJobOnGui()
{
    PendingDetect pending;
    bool hasPending = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_pendingDetect) {
            pending = std::move(*m_pendingDetect);
            m_pendingDetect.reset();
            hasPending = true;
        } else {
            m_busy = false;
        }
    }
    if (hasPending) {
        startJob([this, pending]() {
            runDetectJob(pending.category, pending.image, pending.gen);
        });
        return;
    }
    emit busyChanged(false);
    emit progressChanged(1, 1, QStringLiteral("就绪"));
}
