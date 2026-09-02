import QtQuick

Rectangle {
    id: root
    property string message: ""
    property bool isError: true
    property string pathHint: ""

    visible: message.length > 0
    implicitHeight: col.implicitHeight + 16
    radius: Theme.radius
    color: isError ? Theme.dangerDim : "#332A2208"
    border.color: isError ? Theme.danger : Theme.warn
    border.width: 1

    Column {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 10
        spacing: 4
        Text {
            width: parent.width
            text: root.message
            color: Theme.textPrimary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }
        Text {
            visible: root.pathHint.length > 0
            width: parent.width
            text: root.pathHint
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.monoFamily
            wrapMode: Text.WrapAnywhere
        }
    }
}
