import QtQuick
import QtQuick.Controls
import QtQml.Models

Rectangle {
    id: root
    color: Theme.bgPanel

    Column {
        anchors.fill: parent
        spacing: 0

        Text {
            text: "数据集"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            leftPadding: 12
            topPadding: 10
            bottomPadding: 8
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        TreeView {
            id: tree
            width: parent.width
            height: parent.height - 32
            clip: true
            model: app.datasetModel
            boundsBehavior: Flickable.StopAtBounds
            alternatingRows: false
            selectionBehavior: TableView.SelectRows
            selectionModel: ItemSelectionModel {
                model: app.datasetModel
                onCurrentChanged: function(current, previous) {
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

                readonly property var idx: tree.modelIndex(row, column)
                readonly property string kind: app.datasetModel.nodeType(idx)
                readonly property string title: app.datasetModel.displayName(idx)
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
                            if (del.kind === "defect" && del.title === "good")
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

                onClicked: app.selectFromModelIndex(del.idx)
            }
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
