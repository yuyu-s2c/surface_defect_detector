import QtQuick

Rectangle {
    id: root
    property bool hasImage: false
    property bool detected: false
    property bool verdictOk: true
    property bool liveRunning: false
    property real imageScore: 0
    property real imageThreshold: 0

    readonly property bool positive: !detected

    visible: hasImage
    implicitWidth: Math.max(col.implicitWidth + 24, 108)
    implicitHeight: col.implicitHeight + 16
    radius: Theme.radius
    color: positive ? Theme.accentDim : Theme.dangerDim
    border.width: 1
    border.color: positive ? Theme.accent : Theme.danger
    opacity: 0.96

    Column {
        id: col
        anchors.centerIn: parent
        spacing: 2
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.detected ? "不合格" : "合格"
            color: root.positive ? Theme.accent : Theme.danger
            font.pixelSize: 18
            font.bold: true
            font.family: Theme.fontFamily
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.detected ? "NG · 分数过线" : "OK · 未过线"
            color: Theme.textPrimary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.imageScore.toFixed(3) + (root.detected ? " ≥ " : " < ") + root.imageThreshold.toFixed(3)
            color: Theme.textSecondary
            font.pixelSize: 10
            font.family: Theme.monoFamily
        }
        Text {
            visible: !root.liveRunning
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.verdictOk ? "与 GT 一致" : "与 GT 不一致"
            color: root.verdictOk ? Theme.textSecondary : Theme.warn
            font.pixelSize: 10
            font.family: Theme.fontFamily
        }
    }
}
