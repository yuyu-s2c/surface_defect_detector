import QtQuick
import QtQuick.Layouts

ColumnLayout {
    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        Repeater {
            model: ["缺陷类型", "P", "R", "F1", "IoU", "图像级"]
            Text {
                required property string modelData
                required property int index
                Layout.fillWidth: true
                Layout.preferredWidth: index === 0 ? 90 : (index === 5 ? 110 : 44)
                text: modelData
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.fontFamily
            }
        }
    }

    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: app.metricsModel
        boundsBehavior: Flickable.StopAtBounds
        delegate: Rectangle {
            required property int index
            required property string defect
            required property string precision
            required property string recall
            required property string f1
            required property string iou
            required property string imageAcc
            required property bool isSummary
            width: ListView.view.width
            height: 28
            color: isSummary ? Theme.accentDim : (index % 2 ? "transparent" : "#0AFFFFFF")
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 2
                Text { Layout.preferredWidth: 90; text: defect; color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily; font.bold: isSummary; elide: Text.ElideRight }
                Text { Layout.fillWidth: true; text: precision; color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                Text { Layout.fillWidth: true; text: recall; color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                Text { Layout.fillWidth: true; text: f1; color: Theme.accent; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                Text { Layout.fillWidth: true; text: iou; color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                Text { Layout.preferredWidth: 110; text: imageAcc; color: Theme.textSecondary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily; elide: Text.ElideRight }
            }
        }
    }
}
