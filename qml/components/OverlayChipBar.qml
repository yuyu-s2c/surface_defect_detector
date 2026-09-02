import QtQuick

Row {
    id: root
    spacing: 8
    property bool gtVisible: true
    property bool detVisible: true
    property real zoom: 1
    signal fitRequested()
    signal gtToggled(bool on)
    signal detToggled(bool on)

    Rectangle {
        height: 28
        width: fitText.width + 16
        radius: 14
        color: Theme.bgElevated
        border.color: Theme.border
        Text {
            id: fitText
            anchors.centerIn: parent
            text: "适应画面"
            color: Theme.textPrimary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.fitRequested()
        }
    }

    Rectangle {
        height: 28
        width: zoomText.width + 16
        radius: 14
        color: Theme.bgElevated
        border.color: Theme.border
        Text {
            id: zoomText
            anchors.centerIn: parent
            text: (root.zoom).toFixed(2) + "×"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.monoFamily
        }
    }

    Rectangle {
        height: 28
        width: gtText.width + 20
        radius: 14
        color: root.gtVisible ? "#33FF3B30" : Theme.bgElevated
        border.color: root.gtVisible ? Theme.gtRed : Theme.border
        Text {
            id: gtText
            anchors.centerIn: parent
            text: "GT"
            color: root.gtVisible ? Theme.gtRed : Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            font.bold: true
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.gtToggled(!root.gtVisible)
        }
    }

    Rectangle {
        height: 28
        width: detText.width + 20
        radius: 14
        color: root.detVisible ? "#3300DC00" : Theme.bgElevated
        border.color: root.detVisible ? Theme.detGreen : Theme.border
        Text {
            id: detText
            anchors.centerIn: parent
            text: "检测"
            color: root.detVisible ? Theme.detGreen : Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            font.bold: true
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.detToggled(!root.detVisible)
        }
    }
}
