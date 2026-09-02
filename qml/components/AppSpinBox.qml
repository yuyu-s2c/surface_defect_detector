import QtQuick
import QtQuick.Controls

SpinBox {
    id: control
    editable: true
    font.pixelSize: Theme.bodySize
    font.family: Theme.monoFamily
    implicitWidth: 140
    implicitHeight: 30

    background: Rectangle {
        implicitWidth: 140
        implicitHeight: 30
        radius: Theme.radiusSmall
        color: Theme.bgElevated
        border.color: control.activeFocus ? Theme.accent : Theme.border
        border.width: 1
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        font: control.font
        color: Theme.textPrimary
        selectionColor: Theme.accentDim
        selectedTextColor: Theme.accent
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    up.indicator: Rectangle {
        x: parent.width - width
        height: parent.height
        implicitWidth: 22
        color: control.up.hovered ? Theme.bgHover : "transparent"
        radius: Theme.radiusSmall
        Text {
            anchors.centerIn: parent
            text: "+"
            color: Theme.textSecondary
            font.pixelSize: 12
        }
    }

    down.indicator: Rectangle {
        width: 22
        height: parent.height
        color: control.down.hovered ? Theme.bgHover : "transparent"
        radius: Theme.radiusSmall
        Text {
            anchors.centerIn: parent
            text: "−"
            color: Theme.textSecondary
            font.pixelSize: 12
        }
    }
}
