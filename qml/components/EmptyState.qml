import QtQuick

Item {
    id: root
    property string title: ""
    property string subtitle: ""

    Column {
        anchors.centerIn: parent
        spacing: 8
        width: Math.min(parent.width - 48, 360)
        Text {
            width: parent.width
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: 16
            font.family: Theme.fontFamily
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            text: root.subtitle
            color: Theme.textSecondary
            font.pixelSize: Theme.bodySize
            font.family: Theme.fontFamily
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
