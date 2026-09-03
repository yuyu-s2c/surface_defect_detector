#pragma once

#include <QHash>
#include <QString>

// 数据集文件夹名 → 界面中文。未知 key 原样返回，新类别不必改表。
inline QString folderDisplayName(const QString& key)
{
    static const QHash<QString, QString> kNames = {
        {QStringLiteral("good"), QStringLiteral("良品")},
        {QStringLiteral("bent"), QStringLiteral("弯曲")},
        {QStringLiteral("color"), QStringLiteral("色差")},
        {QStringLiteral("flip"), QStringLiteral("翻转")},
        {QStringLiteral("scratch"), QStringLiteral("划痕")},
        {QStringLiteral("manipulated_front"), QStringLiteral("正面篡改")},
        {QStringLiteral("scratch_head"), QStringLiteral("头部划痕")},
        {QStringLiteral("scratch_neck"), QStringLiteral("颈部划痕")},
        {QStringLiteral("thread_side"), QStringLiteral("侧面螺纹")},
        {QStringLiteral("thread_top"), QStringLiteral("顶部螺纹")},
        {QStringLiteral("metal_nut"), QStringLiteral("金属螺母")},
        {QStringLiteral("screw"), QStringLiteral("螺丝")},
    };
    return kNames.value(key, key);
}
