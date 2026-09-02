import QtQuick

Rectangle {
    id: root
    property int engineKind: 0
    signal picked(int kind)

    implicitWidth: 248
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
                { label: "传统 CV", kind: 0 },
                { label: "EfficientAD", kind: 1 }
            ]
            delegate: Rectangle {
                required property var modelData
                width: (root.width - 8) / 2
                height: root.height - 6
                radius: Theme.radiusSmall
                color: root.engineKind === modelData.kind ? Theme.accent : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    font.pixelSize: Theme.bodySize
                    font.family: Theme.fontFamily
                    font.bold: root.engineKind === modelData.kind
                    color: root.engineKind === modelData.kind ? Theme.bgApp : Theme.textPrimary
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.engineKind !== modelData.kind)
                            root.picked(modelData.kind)
                    }
                }
            }
        }
    }
}
