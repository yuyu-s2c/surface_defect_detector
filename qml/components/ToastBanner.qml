import QtQuick

Rectangle {
    id: root
    property string message: ""

    visible: opacity > 0.01 && message.length > 0
    opacity: 0
    implicitWidth: Math.min(label.implicitWidth + 28, 520)
    implicitHeight: label.implicitHeight + 16
    radius: Theme.radius
    color: Theme.bgElevated
    border.color: Theme.accent
    border.width: 1

    Text {
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, 492)
        text: root.message
        color: Theme.textPrimary
        font.pixelSize: Theme.bodySize
        font.family: Theme.fontFamily
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }

    onMessageChanged: {
        if (message.length === 0) {
            hideAnim.start()
            return
        }
        showAnim.restart()
        hideTimer.restart()
    }

    SequentialAnimation {
        id: showAnim
        NumberAnimation { target: root; property: "opacity"; to: 1; duration: 120 }
    }
    NumberAnimation {
        id: hideAnim
        target: root
        property: "opacity"
        to: 0
        duration: 180
    }
    Timer {
        id: hideTimer
        interval: 2400
        onTriggered: {
            hideAnim.start()
            Qt.callLater(function() { app.clearToast() })
        }
    }
}
