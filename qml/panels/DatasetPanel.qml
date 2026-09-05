import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

Rectangle {
    id: root
    color: Theme.bgPanel
    enabled: !app.liveRunning
    opacity: enabled ? 1 : 0.55

    signal openDatasetRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 8
            Layout.topMargin: 8
            Layout.bottomMargin: 6
            Text {
                text: "数据集"
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.fontFamily
                Layout.fillWidth: true
            }
            AppButton {
                text: "打开…"
                outlined: true
                enabled: !app.busy && !app.liveRunning && auth.canChangeDataset
                onClicked: root.openDatasetRequested()
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        Text {
            visible: app.liveRunning
            Layout.fillWidth: true
            Layout.margins: 8
            text: app.liveSourceKind === 1
                  ? "取流中（WebcamSource）：停止后才能选图"
                  : "模拟取流中（FolderSource，海康离线）：停止后才能选图"
            color: Theme.warn
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }

        TreeView {
            id: tree
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: app.hasDataset
            clip: true
            model: app.datasetModel
            boundsBehavior: Flickable.StopAtBounds
            alternatingRows: false
            selectionBehavior: TableView.SelectRows
            selectionModel: ItemSelectionModel {
                model: app.datasetModel
                onCurrentChanged: function(current, previous) {
                    if (app.liveRunning)
                        return
                    if (current.valid)
                        app.selectFromModelIndex(current)
                }
            }

            delegate: TreeViewDelegate {
                id: del
                implicitWidth: tree.width
                implicitHeight: 32
                leftMargin: 8
                rightMargin: 8
                spacing: 6
                // 不要写死 leftPadding / indentation：模板会按 depth×箭头宽给 contentItem 让位

                readonly property var idx: tree.index(row, column)
                readonly property string kind: app.datasetModel.nodeType(idx)
                readonly property string title: app.datasetModel.displayName(idx)
                readonly property string defectKey: app.datasetModel.defectType(idx)
                readonly property int extraCount: app.datasetModel.nodeCount(idx)

                palette.windowText: Theme.textSecondary
                palette.buttonText: Theme.textPrimary
                palette.highlightedText: Theme.textPrimary

                indicator: Item {
                    implicitWidth: 18
                    implicitHeight: 18
                    x: del.leftMargin + (del.depth * del.indentation)
                    y: (del.height - height) / 2
                    Text {
                        visible: del.hasChildren
                        anchors.centerIn: parent
                        text: del.expanded ? "▾" : "▸"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        font.family: Theme.fontFamily
                    }
                }

                contentItem: Row {
                    spacing: 6
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: del.title
                        color: {
                            if (del.kind === "defect" && del.defectKey === "good")
                                return Theme.textSecondary
                            if (del.kind === "defect")
                                return Theme.warn
                            return Theme.textPrimary
                        }
                        font.pixelSize: Theme.bodySize
                        font.family: Theme.fontFamily
                        font.bold: del.kind === "category"
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        visible: del.extraCount > 0 && del.kind !== "image"
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: countLabel.width + 10
                        implicitHeight: 16
                        radius: 8
                        color: Theme.bgElevated
                        Text {
                            id: countLabel
                            anchors.centerIn: parent
                            text: String(del.extraCount)
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            font.family: Theme.monoFamily
                        }
                    }
                }

                background: Rectangle {
                    color: del.current ? Theme.accentDim
                                       : (del.hovered ? Theme.bgHover : "transparent")
                    Rectangle {
                        width: 2
                        height: parent.height
                        color: Theme.accent
                        visible: del.current
                    }
                }

                onClicked: {
                    if (!app.liveRunning)
                        app.selectFromModelIndex(del.idx)
                }
            }
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !app.hasDataset
            title: "尚未打开数据集"
            subtitle: "选择含 metal_nut、screw 等类别目录的根。每个类别需要 train/good 与 test/。"
        }
    }

    Connections {
        target: app.datasetModel
        function onModelReset() {
            Qt.callLater(function() {
                for (let r = tree.rows - 1; r >= 0; --r) {
                    if (tree.depth(r) === 0)
                        tree.expand(r)
                }
            })
        }
    }
}
