import QtQuick

Rectangle {
    id: root
    property int latencyMs: 0
    property int queueDepth: 0
    property int queueMax: 8
    property real actualFps: 0
    property int okCount: 0
    property int ngCount: 0
    property bool lastNg: false

    implicitWidth: Math.max(col.implicitWidth + 24, 168)
    implicitHeight: col.implicitHeight + 16
    radius: Theme.radius
    color: Theme.bgElevated
    border.color: lastNg ? Theme.danger : Theme.accent
    border.width: 2
    opacity: 0.96

    Column {
        id: col
        anchors.centerIn: parent
        spacing: 4
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "合格  " + root.okCount
            color: Theme.accent
            font.pixelSize: 16
            font.family: Theme.fontFamily
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "不合格  " + root.ngCount
            color: Theme.danger
            font.pixelSize: 16
            font.family: Theme.fontFamily
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.actualFps.toFixed(1) + " 帧/秒  ·  延迟 " + root.latencyMs + " ms"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "队列 " + root.queueDepth + "/" + root.queueMax
            color: root.queueDepth >= root.queueMax ? Theme.warn : Theme.textSecondary
            font.pixelSize: 10
            font.family: Theme.fontFamily
        }
    }
}
