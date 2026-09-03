import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.bgPanel
    height: 76

    signal exportCurrentClicked()
    signal exportBatchClicked()
    signal exportLiveClicked()
    signal datasetClicked()
    signal aboutClicked()

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    Column {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            width: parent.width
            height: 48
            spacing: 8

            Item { width: 12 }

            Text {
                text: "表面缺陷检测"
                color: Theme.textPrimary
                font.pixelSize: Theme.titleSize
                font.bold: true
                font.family: Theme.fontFamily
                Layout.alignment: Qt.AlignVCenter
            }

            ModeSwitch {
                workMode: app.workMode
                enabled: !app.liveRunning && !app.busy
                opacity: enabled ? 1 : 0.5
                onPicked: (m) => app.workMode = m
            }

            Item { Layout.fillWidth: true }

            EngineSwitch {
                engineKind: app.engineKind
                implicitWidth: 200
                enabled: !app.busy && !app.liveRunning
                opacity: enabled ? 1 : 0.5
                onPicked: (k) => app.engineKind = k
            }

            Rectangle {
                visible: app.currentCategory.length > 0
                implicitHeight: 26
                implicitWidth: catText.width + 16
                radius: 13
                color: Theme.bgElevated
                border.color: Theme.border
                Text {
                    id: catText
                    anchors.centerIn: parent
                    text: app.currentCategoryLabel
                    color: Theme.textSecondary
                    font.pixelSize: Theme.smallSize
                    font.family: Theme.fontFamily
                }
            }

            Row {
                spacing: 6
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "帧率"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.smallSize
                    font.family: Theme.fontFamily
                }
                AppSpinBox {
                    from: 1
                    to: 15
                    value: app.liveTargetFps
                    enabled: !app.liveRunning && !app.busy
                    implicitWidth: 78
                    onValueModified: app.liveTargetFps = value
                }
                AppButton {
                    text: app.liveRunning ? "停止" : "模拟开线"
                    primary: !app.liveRunning
                    highlight: app.liveRunning
                    enabled: app.liveRunning || app.canStartLive
                    onClicked: app.liveRunning ? app.stopLive() : app.requestStartLive()
                }
            }

            AppButton {
                id: moreBtn
                text: "更多"
                outlined: true
                active: morePopup.opened
                onClicked: morePopup.opened ? morePopup.close() : morePopup.open()
            }

            Item { width: 10 }
        }

        StationIdentityBar {
            width: parent.width
            height: 28
        }
    }

    // 不用 Controls.Menu：Basic 样式是系统菜单，且 x/y 在 Overlay 坐标系里，
    // 用顶栏本地坐标会飘到窗口左上。Popup 贴按钮右下，走同一套主题。
    Popup {
        id: morePopup
        parent: Overlay.overlay
        padding: 8
        modal: true
        dim: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: 216

        Overlay.modal: Rectangle { color: "transparent" }

        background: Rectangle {
            color: Theme.bgElevated
            border.width: 1
            border.color: Theme.border
            radius: Theme.radius
        }

        onAboutToShow: {
            const gap = 6
            const pt = moreBtn.mapToItem(Overlay.overlay, moreBtn.width, moreBtn.height + gap)
            x = Math.max(8, pt.x - width)
            y = pt.y
        }

        component MenuRow: Item {
            id: row
            property string label: ""
            property string hint: ""
            property bool rowEnabled: true
            signal activated()

            width: morePopup.availableWidth
            implicitHeight: 34
            opacity: rowEnabled ? 1 : 0.38

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusSmall
                color: rowEnabled && hover.containsMouse ? Theme.bgHover : "transparent"
            }
            Text {
                anchors.left: parent.left
                anchors.right: hintLabel.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                text: row.label
                color: Theme.textPrimary
                font.pixelSize: Theme.bodySize
                font.family: Theme.fontFamily
                elide: Text.ElideRight
            }
            Text {
                id: hintLabel
                visible: row.hint.length > 0
                width: visible ? implicitWidth : 0
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 10
                text: row.hint
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.fontFamily
            }
            MouseArea {
                id: hover
                anchors.fill: parent
                hoverEnabled: true
                enabled: row.rowEnabled
                cursorShape: row.rowEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                onClicked: {
                    row.activated()
                    morePopup.close()
                }
            }
        }

        component MenuRule: Rectangle {
            width: morePopup.availableWidth
            height: 1
            color: Theme.border
        }

        Column {
            width: parent.width
            spacing: 2

            MenuRow {
                label: "打开数据集"
                rowEnabled: !app.busy && !app.liveRunning
                onActivated: root.datasetClicked()
            }
            MenuRule {}
            MenuRow {
                label: "导出当前图"
                hint: "PNG"
                rowEnabled: app.hasImage && !app.liveRunning && !app.busy
                onActivated: root.exportCurrentClicked()
            }
            MenuRow {
                label: "导出批量结果"
                hint: "PNG + CSV"
                rowEnabled: app.canExportBatch && !app.liveRunning && !app.busy
                onActivated: root.exportBatchClicked()
            }
            MenuRow {
                label: "导出本班记录"
                rowEnabled: app.canExportLive
                onActivated: root.exportLiveClicked()
            }
            MenuRule {}
            MenuRow {
                label: "关于本工作站"
                onActivated: root.aboutClicked()
            }
        }
    }
}
