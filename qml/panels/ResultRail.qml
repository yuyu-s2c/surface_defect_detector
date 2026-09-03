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

            ColumnLayout {
                visible: !app.liveRunning
                Layout.fillWidth: true
                spacing: 6
                Text {
                    text: "配方"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.smallSize
                    font.family: Theme.fontFamily
                }
                AppTextField {
                    Layout.fillWidth: true
                    text: app.workOrder
                    placeholderText: "工单号"
                    enabled: !app.busy
                    onEditingFinished: app.workOrder = text
                }
                AppTextField {
                    Layout.fillWidth: true
                    text: app.operatorName
                    placeholderText: "操作员"
                    enabled: !app.busy
                    onEditingFinished: app.operatorName = text
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        text: "连续 NG"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.smallSize
                        font.family: Theme.fontFamily
                    }
                    AppSpinBox {
                        Layout.fillWidth: true
                        from: 0
                        to: 200
                        value: app.consecutiveNgLimit
                        enabled: !app.busy
                        onValueModified: app.consecutiveNgLimit = value
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: app.consecutiveNgLimit > 0
                          ? ("达 " + app.consecutiveNgLimit + " 张连续不合格停线；0 关闭")
                          : "联锁关闭，将跑完模拟 playlist"
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    font.family: Theme.fontFamily
                    wrapMode: Text.WordWrap
                }
            }
            Text {
                visible: app.liveRunning
                Layout.fillWidth: true
                text: (app.workOrder.length > 0 ? ("工单 " + app.workOrder + "  ·  ") : "")
                      + "连续 NG " + app.liveConsecutiveNg
                      + (app.consecutiveNgLimit > 0 ? (" / " + app.consecutiveNgLimit) : "（关）")
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.monoFamily
                wrapMode: Text.WordWrap
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
            Text {
                visible: app.liveInterlocked
                Layout.fillWidth: true
                text: app.liveStopReason
                color: Theme.danger
                font.pixelSize: Theme.smallSize
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 8
                StatTile {
                    Layout.fillWidth: true
                    label: "合格"
                    value: String(app.liveOkCount)
                }
                StatTile {
                    Layout.fillWidth: true
                    label: "不合格"
                    value: String(app.liveNgCount)
                }
                StatTile {
                    Layout.fillWidth: true
                    label: "直通率"
                    value: Number(app.liveYieldPercent).toFixed(1) + "%"
                }
                StatTile {
                    Layout.fillWidth: true
                    label: "节拍"
                    value: app.liveTaktMs > 0 ? (app.liveTaktMs + " ms") : "—"
                }
            }
            Text {
                Layout.fillWidth: true
                text: app.consecutiveNgLimit > 0
                      ? ("连续不合格  " + app.liveConsecutiveNg + " / " + app.consecutiveNgLimit)
                      : ("连续不合格  " + app.liveConsecutiveNg + "（联锁关）")
                color: (app.consecutiveNgLimit > 0 && app.liveConsecutiveNg >= app.consecutiveNgLimit)
                       ? Theme.danger : Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.monoFamily
            }
            Rectangle {
                visible: app.consecutiveNgLimit > 0
                Layout.fillWidth: true
                implicitHeight: 6
                radius: 3
                color: Theme.bgElevated
                Rectangle {
                    height: parent.height
                    radius: 3
                    width: parent.width * Math.min(1, app.liveConsecutiveNg / Math.max(app.consecutiveNgLimit, 1))
                    color: Theme.danger
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 4
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

            Text {
                visible: app.doPulseModel.count > 0
                text: "模拟 DO 脉冲"
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.fontFamily
            }
            ListView {
                visible: app.doPulseModel.count > 0
                Layout.fillWidth: true
                implicitHeight: Math.min(app.doPulseModel.count * 36, 108)
                clip: true
                model: app.doPulseModel
                boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    required property int seq
                    required property string point
                    required property string action
                    required property string fileName
                    required property string defectLabel
                    required property bool lateEject
                    width: ListView.view.width
                    height: 36
                    color: "transparent"
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 1
                        Text {
                            width: parent.width
                            text: point + "  " + action + "  " + defectLabel
                            color: Theme.danger
                            font.pixelSize: Theme.smallSize
                            font.family: Theme.monoFamily
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: fileName + (lateEject ? "  ·  迟剔除" : "")
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            font.family: Theme.monoFamily
                            elide: Text.ElideRight
                        }
                    }
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
