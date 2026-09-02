#include "DetectionController.h"

#include "DetectionEngine.h"
#include "DLDetectionEngine.h"

#include <opencv2/imgcodecs.hpp>

#include <QFileInfo>
#include <QtDebug>

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

void DetectionController::setEngineKind(EngineKind kind)
{
    if (m_engineKind == kind)
        return;
    m_engineKind = kind;
    // 引擎类型变了，已缓存的引擎实现全部失效
    qDeleteAll(m_engines);
    m_engines.clear();
}

IDetectionEngine* DetectionController::engineFor(const QString& category)
{
    IDetectionEngine* engine = m_engines.value(category, nullptr);
    if (engine)
        return engine;

    IDetectionEngine* newEngine = nullptr;
    if (m_engineKind == EngineKind::DL) {
        // anomalib Engine.export 实际落点：models/<类别>/weights/onnx/<类别>.onnx
        // （train_efficientad.py 产物）。旧约定 models/<类别>/<类别>.onnx 作回退。
        const QString root = m_dataset.rootPath();
        const QString exported = QStringLiteral("%1/models/%2/weights/onnx/%2.onnx")
                                     .arg(root, category);
        const QString fallback = QStringLiteral("%1/models/%2/%2.onnx")
                                     .arg(root, category);
        const QString modelPath = QFileInfo::exists(exported) ? exported : fallback;
        auto* dl = new DLDetectionEngine(modelPath);
        // metal_nut：k=3、面积门 1000，图像级 0.92、good 误报 4.5%，保持。
        // screw：k 降到 1.0 后图像级 0.69、F1 走平、thread_side 不再跟 k 涨；
        // 细缺陷过线后 totalArea 仍 <1000。面积门改为 300，k 维持 1.0。
        if (category == QStringLiteral("screw")) {
            dl->thresholdSigma = 1.0;
            dl->imageLevelMinArea = 300;
        }
        newEngine = dl;
    } else {
        newEngine = new DetectionEngine;
    }
    if (!newEngine->buildReference(m_dataset.trainGoodImages(category))) {
        delete newEngine;
        return nullptr;
    }
    m_engines.insert(category, newEngine);
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
