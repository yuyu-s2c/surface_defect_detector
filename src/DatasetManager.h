#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

// 数据集加载器（MVTec AD 布局），纯 Qt 实现，不依赖 OpenCV。
// 目录约定：
//   <root>/<产品类>/train/good/*.png           良品参考图
//   <root>/<产品类>/test/<缺陷类型>/*.png       测试图
//   <root>/<产品类>/ground_truth/<缺陷类型>/<原名>_mask.png   像素级标注（good 无）
class DatasetManager
{
public:
    // 扫描数据集根目录（含任意 MVTec 布局类别目录，无白名单）。
    // 只认顶层类别目录（含 test/ 的才算一类）；解压套层目录没有顶层 test/，自然排除。
    // 返回发现的类别数。
    int scan(const QString& datasetRoot);

    QString rootPath() const { return m_root; }
    QStringList categories() const { return m_data.keys(); }

    // 某产品类下的缺陷类型列表（test 子目录名，含 "good"）
    QStringList defectTypes(const QString& category) const;

    // 某产品类某缺陷类型下的测试图片（绝对路径，按文件名排序）
    QStringList testImages(const QString& category, const QString& defectType) const;

    // 某产品类的良品训练图（绝对路径），用于构建参考模板
    QStringList trainGoodImages(const QString& category) const;

    // 测试图对应的 ground_truth 掩码路径；不存在（如 good 类）返回空串
    QString groundTruthMask(const QString& category, const QString& defectType,
                            const QString& testImagePath) const;

    // 在候选目录中寻找数据集根（包含至少一个合法类别目录的目录）
    static QString findDatasetRoot(const QStringList& candidateDirs);

private:
    // 判定一个目录是否是合法的产品类目录（含 test/ 子目录）
    static bool isCategoryDir(const QString& path);

    QString m_root;
    // 类别名 -> (缺陷类型 -> 图片绝对路径列表)
    QMap<QString, QMap<QString, QStringList>> m_data;
    // 类别名 -> train/good 图片列表
    QMap<QString, QStringList> m_trainGood;
};
