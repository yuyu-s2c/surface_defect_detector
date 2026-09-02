import QtQuick

Rectangle {
    id: root
    property int latencyMs: 0
    property int queueDepth: 0
    property int queueMax: 8
    property real actualFps: 0

    implicitWidth: col.implicitWidth + 20
    implicitHeight: col.implicitHeight + 14
    radius: Theme.radius
    color: Theme.bgElevated
    border.color: Theme.border
    opacity: 0.96

    Column {
        id: col
        anchors.centerIn: parent
        spacing: 2
        Text {
            text: "延迟 " + root.latencyMs + " ms"
            color: Theme.textPrimary
            font.pixelSize: Theme.smallSize
            font.family: Theme.monoFamily
        }
        Text {
            text: "队列 " + root.queueDepth + "/" + root.queueMax
            color: root.queueDepth >= root.queueMax ? Theme.warn : Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.monoFamily
        }
        Text {
            text: root.actualFps.toFixed(1) + " fps"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.monoFamily
        }
    }
}
