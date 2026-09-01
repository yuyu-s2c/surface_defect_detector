#include "DetectionController.h"

#include "DetectionEngine.h"

#include <opencv2/imgcodecs.hpp>

DetectionController::DetectionController(QObject* parent)
    : QObject(parent)
{
}

DetectionController::~DetectionController()
{
    qDeleteAll(m_engines);
}

bool DetectionController::loadDataset(const QString& rootPath)
{
    if (m_dataset.scan(rootPath) == 0)
        return false;
    // 数据集换了，已缓存的引擎（基于旧良品图构建）全部失效
    qDeleteAll(m_engines);
    m_engines.clear();
    return true;
}

IDetectionEngine* DetectionController::engineFor(const QString& category)
{
    IDetectionEngine* engine = m_engines.value(category, nullptr);
    if (engine)
        return engine;
    // 当前只有传统 CV 引擎；Phase 2 在此按配置选择 DL 引擎实现
    auto* cvEngine = new DetectionEngine;
    if (!cvEngine->buildReference(m_dataset.trainGoodImages(category))) {
        delete cvEngine;
        return nullptr;
    }
    m_engines.insert(category, cvEngine);
    return cvEngine;
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
    for (const QString& defect : defects) {
        const bool isDefect = (defect != QStringLiteral("good"));
        const QStringList images = m_dataset.testImages(category, defect);
        for (const QString& imgPath : images) {
            cv::Mat img = cv::imread(imgPath.toLocal8Bit().constData(), cv::IMREAD_COLOR);
            if (img.empty())
                continue;
            const DetectionResult r = engine->detect(img);
            const QString gtPath = m_dataset.groundTruthMask(category, defect, imgPath);
            const PixelMetrics pm = ResultEvaluator::evaluatePixel(r.defectMask, gtPath);
            out.pixel[defect] += pm;
            out.image[defect] += ResultEvaluator::evaluateImage(r.detected(), isDefect);
            emit imageProcessed(defect, imgPath, r, pm);
        }
    }
    return true;
}
