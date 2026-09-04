import QtQuick

Rectangle {
    id: root
    property int sourceKind: 0
    signal picked(int kind)

    implicitWidth: 220
    implicitHeight: 34
    radius: Theme.radius
    color: Theme.bgElevated
    border.color: Theme.border
    border.width: 1

    Row {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 2

        Repeater {
            model: [
                { label: "文件夹", kind: 0 },
                { label: "本机摄像头", kind: 1 }
            ]
            delegate: Rectangle {
                required property var modelData
                width: (root.width - 8) / 2
                height: root.height - 6
                radius: Theme.radiusSmall
                color: root.sourceKind === modelData.kind ? Theme.accent : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    font.pixelSize: Theme.bodySize
                    font.family: Theme.fontFamily
                    font.bold: root.sourceKind === modelData.kind
                    color: root.sourceKind === modelData.kind ? Theme.bgApp : Theme.textPrimary
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.sourceKind !== modelData.kind)
                            root.picked(modelData.kind)
                    }
                }
            }
        }
    }
}
