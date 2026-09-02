import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 10
    property int expandedRow: -1

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        StatTile {
            Layout.fillWidth: true
            label: "汇总 F1"
            value: app.hasCompare ? ("CV " + app.compareCvF1 + "  DL " + app.compareDlF1) : "—"
        }
        StatTile {
            Layout.fillWidth: true
            label: "ΔF1 (DL−CV)"
            value: app.hasCompare ? app.compareDeltaF1 : "—"
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        StatTile {
            Layout.fillWidth: true
            label: "图像级 CV / DL"
            value: app.hasCompare ? (app.compareCvImg + "  /  " + app.compareDlImg) : "—"
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        StatTile {
            Layout.fillWidth: true
            label: "good 误报 CV / DL"
            value: app.hasCompare ? (app.compareCvFpr + "  /  " + app.compareDlFpr) : "—"
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Text { Layout.preferredWidth: 72; text: "类型"; color: Theme.textSecondary; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily }
        Text { Layout.fillWidth: true; text: "CV F1"; color: Theme.textSecondary; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily }
        Text { Layout.fillWidth: true; text: "DL F1"; color: Theme.textSecondary; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily }
        Text { Layout.fillWidth: true; text: "ΔF1"; color: Theme.textSecondary; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily }
    }
    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: app.compareModel
        boundsBehavior: Flickable.StopAtBounds
        delegate: Column {
            id: rowRoot
            required property int index
            required property string defect
            required property string cvF1
            required property string dlF1
            required property string deltaF1
            required property string cvPrecision
            required property string cvRecall
            required property string cvIou
            required property string dlPrecision
            required property string dlRecall
            required property string dlIou
            required property string cvImageAcc
            required property string dlImageAcc
            required property bool isSummary
            required property bool isGoodFpr
            width: ListView.view.width
            visible: !isGoodFpr

            Rectangle {
                width: parent.width
                height: 30
                color: isSummary ? Theme.accentDim
                                 : (root.expandedRow === index ? Theme.bgHover
                                    : (index % 2 ? "transparent" : "#0AFFFFFF"))
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    Text { Layout.preferredWidth: 72; text: defect; color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily; font.bold: isSummary; elide: Text.ElideRight }
                    Text { Layout.fillWidth: true; text: cvF1; color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                    Text { Layout.fillWidth: true; text: dlF1; color: Theme.accent; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                    Text { Layout.fillWidth: true; text: deltaF1; color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.expandedRow = (root.expandedRow === index ? -1 : index)
                }
            }

            Rectangle {
                visible: root.expandedRow === index && !isSummary
                width: parent.width
                height: 52
                color: Theme.bgElevated
                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2
                    Text {
                        text: "CV  P " + cvPrecision + "  R " + cvRecall + "  IoU " + cvIou + "  " + cvImageAcc
                        color: Theme.textSecondary
                        font.pixelSize: Theme.smallSize
                        font.family: Theme.monoFamily
                    }
                    Text {
                        text: "DL  P " + dlPrecision + "  R " + dlRecall + "  IoU " + dlIou + "  " + dlImageAcc
                        color: Theme.textSecondary
                        font.pixelSize: Theme.smallSize
                        font.family: Theme.monoFamily
                    }
                }
            }
        }
    }
}
