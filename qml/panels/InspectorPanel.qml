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
                model: ["参数", "指标", "对比"]
                delegate: Item {
                    required property string modelData
                    required property int index
                    width: tabs.width / 3
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

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    ParamForm { Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                }
            }

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
}
