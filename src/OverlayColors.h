#pragma once

#include <QColor>

#include <opencv2/core.hpp>

// GT 红 / 检测绿：画布叠加与 ResultExporter 导出必须同色同透明度。
// alpha 110/255≈0.43、90/255≈0.35，与原 ImageViewWidget 实测观感一致。
namespace OverlayColors {

inline const QColor gtRed{255, 0, 0};
inline const QColor detGreen{0, 220, 0};
inline const QColor detBox{0, 255, 0};

inline constexpr int gtAlpha = 110;
inline constexpr int detAlpha = 90;

inline cv::Scalar gtBgr() { return {0, 0, 255}; }
inline cv::Scalar detBgr() { return {0, 220, 0}; }
inline cv::Scalar detBoxBgr() { return {0, 255, 0}; }

inline constexpr double gtBlend = 110.0 / 255.0;
inline constexpr double detBlend = 90.0 / 255.0;

} // namespace OverlayColors
