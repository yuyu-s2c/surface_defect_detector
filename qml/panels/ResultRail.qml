import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root
    color: Theme.bgPanel

    signal exportCurrentClicked()
    signal exportLiveClicked()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: verdictCol.implicitHeight + 24
            color: !app.hasImage ? Theme.bgElevated
                 : (app.detected ? Theme.dangerDim : Theme.accentDim)
            border.width: 1
            border.color: !app.hasImage ? Theme.border
                        : (app.detected ? Theme.danger : Theme.accent)

            Column {
                id: verdictCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                spacing: 4
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: !app.hasImage ? "等待工件"
                        : (app.detected ? "不合格" : "合格")
                    color: !app.hasImage ? Theme.textSecondary
                         : (app.detected ? Theme.danger : Theme.accent)
                    font.pixelSize: 32
                    font.bold: true
                    font.family: Theme.fontFamily
                }
                Text {
                    visible: app.hasImage
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "检出 " + app.defectCount + " 处"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.bodySize
                    font.family: Theme.fontFamily
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 10

            Text {
                visible: app.hasImage
                Layout.fillWidth: true
                text: "判定依据  分 " + Number(app.imageScore).toFixed(4)
                      + "  /  阈 " + Number(app.imageThreshold).toFixed(4)
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
            }

            Rectangle {
                visible: app.hasImage && app.imageThreshold > 0
                         && app.imageThreshold < 1e12
                Layout.fillWidth: true
                implicitHeight: 6
                radius: 3
                color: Theme.bgElevated
                Rectangle {
                    height: parent.height
                    radius: 3
                    width: parent.width * Math.min(1, app.imageScore / Math.max(app.imageThreshold, 1e-9))
                    color: app.detected ? Theme.danger : Theme.accent
                }
            }

            EngineStatusChip {
                Layout.fillWidth: true
                implicitHeight: 40
            }

            Text {
                visible: app.hasImage
                Layout.fillWidth: true
                text: app.imageInfo
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        ColumnLayout {
            visible: app.hasLiveSession || app.liveRunning
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 8

            Text {
                text: app.liveRunning ? "本班进行中" : "上一班次"
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.fontFamily
            }
            Text {
                visible: app.liveSessionTitle.length > 0
                Layout.fillWidth: true
                text: app.liveSessionTitle
                color: Theme.textPrimary
                font.pixelSize: Theme.bodySize
                font.family: Theme.fontFamily
                font.bold: true
                elide: Text.ElideRight
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 4
                Text { text: "合格"; color: Theme.accent; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily }
                Text { text: String(app.liveOkCount); color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily; font.bold: true }
                Text { text: "不合格"; color: Theme.danger; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily }
                Text { text: String(app.liveNgCount); color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily; font.bold: true }
                Text { text: "丢帧"; color: Theme.textSecondary; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily }
                Text { text: String(app.liveDroppedCount); color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                Text { text: "迟剔除"; color: Theme.textSecondary; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily }
                Text { text: String(app.liveLateCount); color: Theme.textPrimary; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                Text { text: "延迟"; color: Theme.textSecondary; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily }
                Text {
                    text: app.liveLatencyMs + " ms（峰值 " + app.liveMaxLatencyMs + "）"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.smallSize
                    font.family: Theme.monoFamily
                }
                Text { text: "队列"; color: Theme.textSecondary; font.pixelSize: Theme.smallSize; font.family: Theme.fontFamily }
                Text {
                    text: app.liveQueueDepth + "/" + app.liveQueueMax
                    color: Theme.textPrimary
                    font.pixelSize: Theme.smallSize
                    font.family: Theme.monoFamily
                }
            }
        }

        Text {
            visible: app.hasLiveSession || app.liveRunning
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 4
            text: "不合格"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }

        Item {
            visible: app.hasLiveSession || app.liveRunning
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: ngList
                anchors.fill: parent
                clip: true
                model: app.ngModel
                boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    required property int index
                    required property string fileName
                    required property string defectLabel
                    required property string scoreText
                    required property int latencyMs
                    required property bool lateEject
                    required property bool rowSelected
                    width: ngList.width
                    height: 48
                    color: rowSelected ? Theme.accentDim
                         : (index % 2 === 1 ? Theme.tableStripe : "transparent")

                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 2
                        Text {
                            width: parent.width
                            text: defectLabel + "  " + fileName
                            color: Theme.textPrimary
                            font.pixelSize: Theme.bodySize
                            font.family: Theme.fontFamily
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: "分 " + scoreText + "  ·  " + latencyMs + " ms"
                                  + (lateEject ? "  ·  迟剔除" : "")
                            color: lateEject ? Theme.warn : Theme.textSecondary
                            font.pixelSize: Theme.smallSize
                            font.family: Theme.monoFamily
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !app.liveRunning && !app.busy
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: app.reviewNg(index)
                    }
                }
            }

            EmptyState {
                anchors.fill: parent
                visible: app.ngModel.count === 0
                title: app.liveRunning ? "尚无不合格" : "本班没有不合格"
                subtitle: app.liveRunning ? "分数过线的工件会出现在这里。" : "停流后可点选回看。"
            }
        }

        BoxTable {
            visible: !app.liveRunning && !app.hasLiveSession
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        AppButton {
            visible: !app.hasLiveSession && !app.liveRunning
            Layout.fillWidth: true
            Layout.margins: 12
            text: "导出当前图"
            outlined: true
            enabled: app.hasImage && !app.busy && !app.liveRunning
            onClicked: root.exportCurrentClicked()
        }

        AppButton {
            visible: app.canExportLive
            Layout.fillWidth: true
            Layout.margins: 12
            text: "导出班次"
            outlined: true
            enabled: app.canExportLive && !app.busy
            onClicked: root.exportLiveClicked()
        }
    }
}
