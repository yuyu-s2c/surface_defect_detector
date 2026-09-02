#pragma once

#include "ResultEvaluator.h"

#include <QString>

inline QString fmt3(double v)
{
    return QString::number(v, 'f', 3);
}

inline QString imageAccText(const ImageMetrics& im)
{
    return QStringLiteral("%1 (%2/%3)").arg(fmt3(im.accuracy())).arg(im.correct).arg(im.total);
}
