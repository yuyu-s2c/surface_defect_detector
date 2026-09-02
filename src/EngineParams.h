#pragma once

#include <QString>

// 传统 CV / DL 引擎 GUI 可调参数（Phase 3）。
// 默认值与 Phase 2 收住的工作点一致，保证未改参时 --batch 口径不漂。
// gaussianKernel / stdEps / aggWindow / inputSize 仍是算法常量，不开放。

struct TraditionalParams
{
    double zAggThreshold = 1.4;   // 聚合分数阈值
    int morphCloseKernel = 21;    // 闭运算核
    int minDefectArea = 100;      // 连通域最小面积（像素）
    int imageLevelMinArea = 1000; // 面积门；CV 的图像分就是 totalArea

    static TraditionalParams defaults() { return {}; }
};

struct DLParams
{
    double thresholdSigma = 3.0;  // 像素阈值 = 良品热图最大值均值 + kσ（只切掩码）
    int morphCloseKernel = 21;
    int minDefectArea = 100;
    int imageLevelMinArea = 1000; // 叠加/框面积门，对照列用；不再驱动图像级判定

    // 类别无关默认（新类接入走这里：k=3 / 面积门 1000 + 图像级分数标定）
    static DLParams defaults() { return {}; }

    // CLI `--batch` 不读 QSettings，P2 已收住的两类工作点必须留在代码里。
    // 新类别禁止再加 if (category == ...)，一律 defaults()。
    static DLParams defaultsFor(const QString& category)
    {
        if (category == QStringLiteral("screw")) {
            DLParams p;
            p.thresholdSigma = 1.0;
            p.imageLevelMinArea = 300;
            return p;
        }
        return defaults();
    }
};
