import QtQuick

Rectangle {
    id: root
    property int workMode: 0
    signal picked(int mode)

    implicitWidth: 132
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
                { label: "检测", mode: 0 },
                { label: "分析", mode: 1 }
            ]
            delegate: Rectangle {
                required property var modelData
                width: (root.width - 8) / 2
                height: root.height - 6
                radius: Theme.radiusSmall
                color: root.workMode === modelData.mode ? Theme.accent : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    font.pixelSize: Theme.bodySize
                    font.family: Theme.fontFamily
                    font.bold: root.workMode === modelData.mode
                    color: root.workMode === modelData.mode ? Theme.bgApp : Theme.textPrimary
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.workMode !== modelData.mode)
                            root.picked(modelData.mode)
                    }
                }
            }
        }
    }
}
