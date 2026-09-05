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
    signal passwordClicked()
    signal usersClicked()
    signal logoutClicked()

    readonly property bool inspectMode: app.workMode === 0

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
                visible: root.width >= 1040
                text: "表面缺陷检测"
                color: Theme.textPrimary
                font.pixelSize: Theme.titleSize
                font.bold: true
                font.family: Theme.fontFamily
                elide: Text.ElideRight
                Layout.maximumWidth: 128
                Layout.alignment: Qt.AlignVCenter
            }

            ModeSwitch {
                workMode: app.workMode
                analyzeEnabled: auth.canAnalyze
                enabled: !app.liveRunning && !app.busy
                opacity: enabled ? 1 : 0.5
                onPicked: (m) => app.workMode = m
            }

            Item { Layout.fillWidth: true }

            SourceSwitch {
                visible: root.inspectMode
                sourceKind: app.liveSourceKind
                implicitWidth: 188
                enabled: !app.busy && !app.liveRunning
                opacity: enabled ? 1 : 0.5
                onPicked: (k) => app.liveSourceKind = k
            }

            EngineSwitch {
                visible: !root.inspectMode
                engineKind: app.engineKind
                implicitWidth: 200
                enabled: !app.busy && !app.liveRunning && auth.canChangeEngine
                opacity: enabled ? 1 : 0.5
                onPicked: (k) => app.engineKind = k
            }

            AppButton {
                text: app.liveRunning ? "停止" : app.liveStartButtonText
                primary: !app.liveRunning
                highlight: app.liveRunning
                enabled: app.liveRunning || app.canStartLive
                onClicked: app.liveRunning ? app.stopLive() : app.requestStartLive()
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
                anchors.right: hintLabel.visible ? hintLabel.left : parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 10
                anchors.rightMargin: hintLabel.visible ? 8 : 10
                text: row.label
                color: Theme.textPrimary
                font.pixelSize: Theme.bodySize
                font.family: Theme.fontFamily
                elide: Text.ElideRight
            }
            Text {
                id: hintLabel
                visible: row.hint.length > 0
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
                rowEnabled: !app.busy && !app.liveRunning && auth.canChangeDataset
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
                rowEnabled: app.canExportBatch && !app.liveRunning && !app.busy && auth.canAnalyze
                onActivated: root.exportBatchClicked()
            }
            MenuRow {
                label: "导出本班记录"
                rowEnabled: app.canExportLive
                onActivated: root.exportLiveClicked()
            }
            MenuRule {}
            MenuRow {
                label: "修改密码"
                rowEnabled: auth.loggedIn
                onActivated: root.passwordClicked()
            }
            MenuRow {
                label: "用户管理"
                rowEnabled: auth.canManageUsers && !app.liveRunning && !app.busy
                onActivated: root.usersClicked()
            }
            MenuRow {
                label: "退出登录"
                rowEnabled: auth.loggedIn && !app.busy
                onActivated: root.logoutClicked()
            }
            MenuRule {}
            MenuRow {
                label: "关于本工作站"
                onActivated: root.aboutClicked()
            }
        }
    }
}
