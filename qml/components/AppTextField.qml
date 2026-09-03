import QtQuick
import QtQuick.Controls

TextField {
    id: root
    color: Theme.textPrimary
    placeholderTextColor: Theme.textSecondary
    font.pixelSize: Theme.bodySize
    font.family: Theme.fontFamily
    selectByMouse: true
    leftPadding: 8
    rightPadding: 8
    implicitHeight: 30

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.bgElevated
        border.width: 1
        border.color: root.activeFocus ? Theme.accent : Theme.border
    }
}
