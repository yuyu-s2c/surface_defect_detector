pragma Singleton
import QtQuick

QtObject {
    readonly property color bgApp: "#0F1419"
    readonly property color bgPanel: "#161C22"
    readonly property color bgElevated: "#1C242C"
    readonly property color bgCanvas: "#0B0F14"
    readonly property color bgHover: "#24303A"
    readonly property color border: "#2A3540"
    readonly property color textPrimary: "#E7EEF4"
    readonly property color textSecondary: "#8B9AAB"
    readonly property color accent: "#3DDC97"
    readonly property color accentDim: "#1A3D30"
    readonly property color danger: "#FF5C5C"
    readonly property color dangerDim: "#3A1C1C"
    readonly property color gtRed: "#FF3B30"
    readonly property color detGreen: "#00DC00"
    readonly property color warn: "#F5C542"

    readonly property int titleSize: 16
    readonly property int bodySize: 13
    readonly property int smallSize: 11
    readonly property int statSize: 20

    readonly property string fontFamily: "Microsoft YaHei UI"
    readonly property string monoFamily: "Consolas"

    readonly property int radius: 8
    readonly property int radiusSmall: 6
    readonly property int pad: 12
    readonly property int gap: 8

    readonly property int tableRowH: 30
    readonly property int tableHeaderH: 30
    readonly property int tablePad: 10
    readonly property color tableStripe: "#0DFFFFFF"
    readonly property color tableGrid: "#1A2A3540"
}
