import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property bool active: false
    property string text: ""
    property string subtitle: ""
    property int current: 0
    property int total: 0
    property bool calibrating: false

    visible: active
    color: "#CC0B0F14"

    Column {
        anchors.centerIn: parent
        spacing: 12
        width: Math.min(parent.width - 48, 420)
        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: root.active
            width: 36
            height: 36
            palette.dark: root.calibrating ? Theme.warn : Theme.accent
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.text.length ? root.text : "处理中…"
            color: Theme.textPrimary
            font.pixelSize: Theme.bodySize
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }
        Text {
            visible: root.subtitle.length > 0
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.subtitle
            color: root.calibrating ? Theme.warn : Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }
        Rectangle {
            visible: root.total > 0
            anchors.horizontalCenter: parent.horizontalCenter
            width: 220
            height: 6
            radius: 3
            color: Theme.bgElevated
            Rectangle {
                width: root.total > 0 ? parent.width * Math.min(1, root.current / root.total) : 0
                height: parent.height
                radius: 3
                color: root.calibrating ? Theme.warn : Theme.accent
            }
        }
    }
}
