import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 10

    readonly property var extra: app.compareModel.extraAt(table.selectedRow)

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        StatTile {
            Layout.fillWidth: true
            label: "汇总综合分"
            value: app.hasCompare ? ("传统 " + app.compareCvF1 + "  深度 " + app.compareDlF1) : "—"
        }
        StatTile {
            Layout.fillWidth: true
            label: "深度相对传统"
            value: app.hasCompare ? app.compareDeltaF1 : "—"
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        StatTile {
            Layout.fillWidth: true
            label: "图像检出 传统 / 深度"
            value: app.hasCompare ? (app.compareCvImg + "  /  " + app.compareDlImg) : "—"
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        StatTile {
            Layout.fillWidth: true
            label: "良品误报 传统 / 深度"
            value: app.hasCompare ? (app.compareCvFpr + "  /  " + app.compareDlFpr) : "—"
        }
    }

    AppTable {
        id: table
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: app.compareModel
        columnWeight: [1.25, 1.1, 1.1, 0.9]
        columnAlign: [Text.AlignLeft, Text.AlignRight, Text.AlignRight, Text.AlignRight]
        textColumns: [0]
        accentColumns: [2]
        selectable: true
    }

    Rectangle {
        visible: table.selectedRow >= 0 && extra && extra.isSummary !== true
        Layout.fillWidth: true
        implicitHeight: detailGrid.implicitHeight + 16
        radius: Theme.radius
        color: Theme.bgElevated
        border.color: Theme.border
        border.width: 1

        GridLayout {
            id: detailGrid
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 8
            columns: 5
            columnSpacing: 8
            rowSpacing: 4

            Repeater {
                model: ["", "精确率", "召回率", "交并比", "图像检出"]
                Text {
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true
                    text: modelData
                    color: Theme.textSecondary
                    font.pixelSize: Theme.smallSize
                    font.family: Theme.fontFamily
                    horizontalAlignment: index === 0 ? Text.AlignLeft : Text.AlignRight
                }
            }
            Repeater {
                model: [
                    "传统",
                    extra.cvPrecision ?? "",
                    extra.cvRecall ?? "",
                    extra.cvIou ?? "",
                    extra.cvImageAcc ?? ""
                ]
                Text {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    text: modelData
                    color: Theme.textPrimary
                    font.pixelSize: Theme.smallSize
                    font.family: index === 0 ? Theme.fontFamily : Theme.monoFamily
                    horizontalAlignment: index === 0 ? Text.AlignLeft : Text.AlignRight
                    elide: Text.ElideRight
                }
            }
            Repeater {
                model: [
                    "深度",
                    extra.dlPrecision ?? "",
                    extra.dlRecall ?? "",
                    extra.dlIou ?? "",
                    extra.dlImageAcc ?? ""
                ]
                Text {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    text: modelData
                    color: index === 0 ? Theme.textPrimary : Theme.accent
                    font.pixelSize: Theme.smallSize
                    font.family: index === 0 ? Theme.fontFamily : Theme.monoFamily
                    horizontalAlignment: index === 0 ? Text.AlignLeft : Text.AlignRight
                    elide: Text.ElideRight
                }
            }
        }
    }
}
