import QtQuick

Rectangle {
    id: root
    property bool hasImage: false
    property bool detected: false
    property bool verdictOk: true

    visible: hasImage
    implicitWidth: col.implicitWidth + 24
    implicitHeight: col.implicitHeight + 16
    radius: Theme.radius
    color: !hasImage ? "transparent"
                     : (verdictOk ? Theme.accentDim : Theme.dangerDim)
    border.width: 1
    border.color: !hasImage ? "transparent"
                            : (verdictOk ? Theme.accent : Theme.danger)
    opacity: 0.96

    Column {
        id: col
        anchors.centerIn: parent
        spacing: 2
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.detected ? "检出" : "未检出"
            color: root.verdictOk ? Theme.accent : Theme.danger
            font.pixelSize: 16
            font.bold: true
            font.family: Theme.fontFamily
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.verdictOk ? "判定正确" : "误判"
            color: Theme.textPrimary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }
    }
}
