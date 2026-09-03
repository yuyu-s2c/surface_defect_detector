import QtQuick

Rectangle {
    id: root
    property string statusText: "就绪"
    property string tone: "normal"
    property bool busy: false
    property int current: 0
    property int total: 0
    property int liveOk: 0
    property int liveNg: 0
    property bool liveRunning: false

    color: Theme.bgPanel
    height: 32

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }

    Rectangle {
        width: 3
        height: parent.height
        color: {
            if (root.tone === "ng")
                return Theme.danger
            if (root.tone === "ok")
                return Theme.accent
            if (root.tone === "warn" || root.tone === "busy")
                return Theme.warn
            return "transparent"
        }
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: pills.left
        anchors.rightMargin: 12
        text: root.statusText
        color: root.tone === "ng" ? Theme.danger
             : (root.tone === "ok" ? Theme.accent : Theme.textSecondary)
        font.pixelSize: Theme.smallSize
        font.family: Theme.fontFamily
        elide: Text.ElideRight
    }

    Row {
        id: pills
        anchors.right: bar.left
        anchors.rightMargin: root.busy ? 12 : 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        visible: root.liveRunning
        Text {
            text: "合格 " + root.liveOk
            color: Theme.accent
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            font.bold: true
        }
        Text {
            text: "不合格 " + root.liveNg
            color: Theme.danger
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            font.bold: true
        }
    }

    Rectangle {
        id: bar
        visible: root.busy
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        width: 180
        height: 6
        radius: 3
        color: Theme.bgElevated

        Rectangle {
            id: fill
            height: parent.height
            radius: 3
            color: Theme.accent
            width: root.total > 0 ? parent.width * Math.min(1, root.current / root.total) : parent.width * 0.35

            SequentialAnimation on x {
                running: root.busy && root.total <= 0
                loops: Animation.Infinite
                NumberAnimation { from: -40; to: 140; duration: 900 }
            }
        }
    }
}
