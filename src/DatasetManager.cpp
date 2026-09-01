#include "DatasetManager.h"

#include <QDir>
#include <QFileInfo>

static const QStringList kImageFilters = {QStringLiteral("*.png")};

int DatasetManager::scan(const QString& datasetRoot)
{
    m_data.clear();
    m_trainGood.clear();
    m_root.clear();

    QDir root(datasetRoot);
    if (!root.exists())
        return 0;

    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& cat : entries) {
        const QString catPath = root.absoluteFilePath(cat);
        if (!isCategoryDir(catPath))
            continue;

        // test/<缺陷类型>/*.png
        QDir testDir(catPath + QStringLiteral("/test"));
        const QStringList defectDirs = testDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        QMap<QString, QStringList> defectMap;
        for (const QString& defect : defectDirs) {
            QDir d(testDir.absoluteFilePath(defect));
            QStringList images;
            const QFileInfoList files = d.entryInfoList(kImageFilters, QDir::Files, QDir::Name);
            for (const QFileInfo& f : files)
                images << f.absoluteFilePath();
            if (!images.isEmpty())
                defectMap.insert(defect, images);
        }
        if (defectMap.isEmpty())
            continue;
        m_data.insert(cat, defectMap);

        // train/good/*.png
        QDir goodDir(catPath + QStringLiteral("/train/good"));
        QStringList goodImages;
        const QFileInfoList goodFiles = goodDir.entryInfoList(kImageFilters, QDir::Files, QDir::Name);
        for (const QFileInfo& f : goodFiles)
            goodImages << f.absoluteFilePath();
        m_trainGood.insert(cat, goodImages);
    }

    if (!m_data.isEmpty())
        m_root = root.absolutePath();
    return m_data.size();
}

QStringList DatasetManager::defectTypes(const QString& category) const
{
    return m_data.value(category).keys();
}

QStringList DatasetManager::testImages(const QString& category, const QString& defectType) const
{
    return m_data.value(category).value(defectType);
}

QStringList DatasetManager::trainGoodImages(const QString& category) const
{
    return m_trainGood.value(category);
}

QString DatasetManager::groundTruthMask(const QString& category, const QString& defectType,
                                        const QString& testImagePath) const
{
    if (m_root.isEmpty())
        return QString();
    // 000.png -> 000_mask.png
    const QString base = QFileInfo(testImagePath).completeBaseName();
    const QString maskPath = m_root + QLatin1Char('/') + category
        + QStringLiteral("/ground_truth/") + defectType
        + QLatin1Char('/') + base + QStringLiteral("_mask.png");
    return QFileInfo::exists(maskPath) ? maskPath : QString();
}

QString DatasetManager::findDatasetRoot(const QStringList& candidateDirs)
{
    for (const QString& dirPath : candidateDirs) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;
        const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& e : entries) {
            if (isCategoryDir(dir.absoluteFilePath(e)))
                return dir.absolutePath();
        }
    }
    return QString();
}

bool DatasetManager::isCategoryDir(const QString& path)
{
    // 合法类别目录：含 test/ 且 test/ 下至少有一个含图片的子目录。
    // 嵌套重复目录（metal_nut/metal_nut）不含 test/，自然被排除。
    QDir testDir(path + QStringLiteral("/test"));
    if (!testDir.exists())
        return false;
    const QStringList subDirs = testDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& sub : subDirs) {
        QDir d(testDir.absoluteFilePath(sub));
        if (!d.entryList(kImageFilters, QDir::Files).isEmpty())
            return true;
    }
    return false;
}
