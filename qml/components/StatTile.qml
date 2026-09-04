import QtQuick

Rectangle {
    id: root
    property string label: ""
    property string value: "—"
    property bool compact: false
    implicitHeight: compact ? 52 : 64
    radius: Theme.radius
    color: Theme.bgElevated
    border.color: Theme.border
    border.width: 1

    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 4
        Text {
            text: root.label
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }
        Text {
            text: root.value
            color: Theme.textPrimary
            font.pixelSize: root.compact ? 16 : Theme.statSize
            font.family: Theme.monoFamily
            font.bold: true
            elide: Text.ElideRight
            width: parent.width
        }
    }
}
