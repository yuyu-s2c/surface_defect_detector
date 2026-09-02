import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property bool active: false
    property string text: ""
    property int current: 0
    property int total: 0

    visible: active
    color: "#CC0B0F14"

    Column {
        anchors.centerIn: parent
        spacing: 12
        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: root.active
            width: 36
            height: 36
            palette.dark: Theme.accent
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.text.length ? root.text : "处理中…"
            color: Theme.textPrimary
            font.pixelSize: Theme.bodySize
            font.family: Theme.fontFamily
        }
        Rectangle {
            visible: root.total > 0
            width: 220
            height: 6
            radius: 3
            color: Theme.bgElevated
            Rectangle {
                width: root.total > 0 ? parent.width * Math.min(1, root.current / root.total) : 0
                height: parent.height
                radius: 3
                color: Theme.accent
            }
        }
    }
}
