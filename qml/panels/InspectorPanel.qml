import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root
    color: Theme.bgPanel

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Row {
            id: tabs
            Layout.fillWidth: true
            height: 40
            Repeater {
                model: ["当前", "参数", "指标", "对比"]
                delegate: Item {
                    required property string modelData
                    required property int index
                    width: tabs.width / 4
                    height: 40
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: app.inspectorTab === index ? Theme.accent : Theme.textSecondary
                        font.pixelSize: Theme.bodySize
                        font.family: Theme.fontFamily
                        font.bold: app.inspectorTab === index
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 2
                        color: app.inspectorTab === index ? Theme.accent : "transparent"
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: app.inspectorTab = index
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: app.inspectorTab

            // 当前
            ColumnLayout {
                anchors.margins: 0
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10
                        Text {
                            text: app.imageInfo
                            color: Theme.textPrimary
                            font.pixelSize: Theme.bodySize
                            font.family: Theme.fontFamily
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            StatTile { Layout.fillWidth: true; label: "缺陷框"; value: app.hasImage ? String(app.defectCount) : "—" }
                            StatTile { Layout.fillWidth: true; label: "总面积"; value: app.hasImage ? Number(app.totalArea).toFixed(0) : "—" }
                            StatTile { Layout.fillWidth: true; label: "叠加面积门"; value: app.hasImage ? String(app.minImageArea) : "—" }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            StatTile { Layout.fillWidth: true; label: "图像分"; value: app.hasImage ? Number(app.imageScore).toFixed(4) : "—" }
                            StatTile { Layout.fillWidth: true; label: "判定阈值"; value: app.hasImage ? Number(app.imageThreshold).toFixed(4) : "—" }
                        }
                        BoxTable { Layout.fillWidth: true; Layout.fillHeight: true }
                        AppButton {
                            Layout.fillWidth: true
                            text: "导出当前图"
                            outlined: true
                            enabled: app.hasImage
                            onClicked: root.exportCurrentClicked()
                        }
                    }
                }
            }

            // 参数
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    ParamForm { Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                }
            }

            // 指标
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10
                    AppButton {
                        Layout.fillWidth: true
                        text: "批量运行当前类别"
                        primary: true
                        enabled: app.canRunBatch
                        onClicked: app.runBatch()
                    }
                    MetricsTable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: app.hasMetrics
                    }
                    EmptyState {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: !app.hasMetrics
                        title: "还没有批量指标"
                        subtitle: "先对当前类别跑批量，才能看到本引擎的 P / R / F1。"
                    }
                }
            }

            // 对比
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10
                    AppButton {
                        Layout.fillWidth: true
                        text: "对比双引擎"
                        outlined: true
                        enabled: app.canRunBatch
                        onClicked: app.compareEngines()
                    }
                    CompareView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: app.hasCompare
                    }
                    EmptyState {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: !app.hasCompare
                        title: "还没有对比结果"
                        subtitle: "点上方按钮。缺哪侧批量就补跑哪侧，口径与 --batch 相同。"
                    }
                }
            }
        }
    }

    signal exportCurrentClicked()
}
