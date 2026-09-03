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

            // 当前：判定优先，分数与框表退到后面
            ColumnLayout {
                anchors.margins: 0
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        Rectangle {
                            visible: app.hasImage
                            Layout.fillWidth: true
                            implicitHeight: 80
                            radius: Theme.radius
                            color: app.detected ? Theme.dangerDim : Theme.accentDim
                            border.width: 1
                            border.color: app.detected ? Theme.danger : Theme.accent
                            Column {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: app.detected ? "不合格" : "合格"
                                    color: app.detected ? Theme.danger : Theme.accent
                                    font.pixelSize: 28
                                    font.bold: true
                                    font.family: Theme.fontFamily
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "检出 " + app.defectCount + " 处"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.bodySize
                                    font.family: Theme.fontFamily
                                }
                            }
                        }

                        Text {
                            text: app.imageInfo
                            color: Theme.textPrimary
                            font.pixelSize: Theme.bodySize
                            font.family: Theme.fontFamily
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        EngineStatusChip {
                            Layout.fillWidth: true
                            implicitHeight: 40
                        }
                        Text {
                            visible: app.hasImage
                            text: "判定依据  分 " + Number(app.imageScore).toFixed(4)
                                  + "  /  阈 " + Number(app.imageThreshold).toFixed(4)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.smallSize
                            font.family: Theme.fontFamily
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        BoxTable { Layout.fillWidth: true; Layout.fillHeight: true }
                        AppButton {
                            Layout.fillWidth: true
                            text: "导出当前图"
                            outlined: true
                            enabled: app.hasImage && !app.busy && !app.liveRunning
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
                        subtitle: app.engineKind === 1 && !app.modelAvailable
                                  ? "当前类别没有 ONNX，先放到约定路径或改用传统 CV。"
                                  : "先对当前类别跑批量，才能看到精确率、召回率和综合分。"
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
