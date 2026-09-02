#pragma once

#include <QString>
#include <QtGlobal>

// 传统 CV / DL 引擎 GUI 可调参数（Phase 3）。
// 默认值与 Phase 2 收住的工作点一致，保证未改参时 --batch 口径不漂。
// gaussianKernel / stdEps / aggWindow / inputSize 仍是算法常量，不开放。
//
// sanitize()：把 QSettings / 旋钮里的脏值夹到 OpenCV 能吃的范围。
// P2 工作点（CV 1.4/21/100/1000；metal_nut k=3/1000；screw k=1/300）都在区间内，夹紧是恒等。

inline int sanitizedMorphKernel(int v, int lo = 1, int hi = 51)
{
    v = qBound(lo, v, hi);
    if ((v % 2) == 0)
        v = (v + 1 <= hi) ? (v + 1) : (v - 1);
    return v;
}

struct TraditionalParams
{
    double zAggThreshold = 1.4;   // 聚合分数阈值
    int morphCloseKernel = 21;    // 闭运算核
    int minDefectArea = 100;      // 连通域最小面积（像素）
    int imageLevelMinArea = 1000; // 面积门；CV 的图像分就是 totalArea

    static TraditionalParams defaults() { return {}; }

    TraditionalParams sanitized() const
    {
        TraditionalParams p = *this;
        p.zAggThreshold = qBound(0.1, p.zAggThreshold, 10.0);
        p.morphCloseKernel = sanitizedMorphKernel(p.morphCloseKernel);
        p.minDefectArea = qBound(0, p.minDefectArea, 100000);
        p.imageLevelMinArea = qBound(0, p.imageLevelMinArea, 1000000);
        return p;
    }
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

    DLParams sanitized() const
    {
        DLParams p = *this;
        p.thresholdSigma = qBound(0.1, p.thresholdSigma, 8.0);
        p.morphCloseKernel = sanitizedMorphKernel(p.morphCloseKernel);
        p.minDefectArea = qBound(0, p.minDefectArea, 100000);
        p.imageLevelMinArea = qBound(0, p.imageLevelMinArea, 1000000);
        return p;
    }
};
