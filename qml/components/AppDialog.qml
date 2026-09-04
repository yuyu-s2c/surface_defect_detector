import QtQuick
import QtQuick.Controls

// 工位主题对话框。系统 Dialog 默认是灰底控件，和暗色 HMI 不一致。
Dialog {
    id: root
    modal: true
    padding: 16
    anchors.centerIn: parent
    palette.window: Theme.bgElevated
    palette.windowText: Theme.textPrimary
    palette.button: Theme.bgHover
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.bgApp

    background: Rectangle {
        color: Theme.bgElevated
        border.width: 1
        border.color: Theme.border
        radius: Theme.radius
    }

    Overlay.modal: Rectangle {
        color: "#B30B0F14"
    }

    header: Rectangle {
        visible: root.title.length > 0
        implicitHeight: 48
        color: "transparent"

        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: Theme.titleSize
            font.bold: true
            font.family: Theme.fontFamily
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.border
        }
    }

    footer: Item {
        visible: (root.standardButtons & Dialog.Ok) !== 0
        implicitHeight: 56

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: Theme.border
        }

        AppButton {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 16
            text: "确定"
            primary: true
            onClicked: root.accept()
        }
    }
}
