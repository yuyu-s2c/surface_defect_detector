import QtQuick

Rectangle {
    id: root
    property bool hasImage: false
    property bool detected: false
    property bool verdictOk: true
    property bool liveRunning: false
    property bool gtVisible: false
    property real imageScore: 0
    property real imageThreshold: 0

    readonly property bool positive: !detected

    visible: hasImage
    implicitWidth: Math.max(col.implicitWidth + 28, 132)
    implicitHeight: col.implicitHeight + 20
    radius: Theme.radius
    color: positive ? Theme.accentDim : Theme.dangerDim
    border.width: 2
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
            font.pixelSize: 28
            font.bold: true
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
            visible: !root.liveRunning && root.gtVisible
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.verdictOk ? "与真值一致" : "与真值不一致"
            color: root.verdictOk ? Theme.textSecondary : Theme.warn
            font.pixelSize: 10
            font.family: Theme.fontFamily
        }
    }
}
