import QtQuick
import QtQuick.Layouts

ColumnLayout {
    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        spacing: 0
        Repeater {
            model: ["#", "x", "y", "宽", "高", "面积"]
            Text {
                required property string modelData
                required property int index
                Layout.fillWidth: index > 0
                Layout.preferredWidth: index === 0 ? 28 : -1
                text: modelData
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.fontFamily
            }
        }
    }

    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: list
            anchors.fill: parent
            clip: true
            model: app.boxModel
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                required property int index
                width: ListView.view.width
                height: 26
                color: index % 2 ? "transparent" : "#0AFFFFFF"
                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    Text { Layout.preferredWidth: 28; text: String(index + 1); color: Theme.textSecondary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                    Text { Layout.fillWidth: true; text: String(model.x); color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                    Text { Layout.fillWidth: true; text: String(model.y); color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                    Text { Layout.fillWidth: true; text: String(model.width); color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                    Text { Layout.fillWidth: true; text: String(model.height); color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                    Text { Layout.fillWidth: true; text: Number(model.area).toFixed(0); color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                }
            }
        }

        EmptyState {
            visible: app.defectCount === 0
            anchors.fill: parent
            title: app.hasImage ? "未检出缺陷框" : "未选择图片"
            subtitle: ""
        }
    }
}
