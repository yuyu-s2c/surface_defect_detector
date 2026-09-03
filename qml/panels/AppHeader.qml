import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.bgPanel
    height: 48

    signal exportCurrentClicked()
    signal exportBatchClicked()
    signal datasetClicked()
    signal aboutClicked()

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 12
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 10

        Column {
            spacing: 0
            Layout.alignment: Qt.AlignVCenter
            Text {
                text: "表面缺陷检测"
                color: Theme.textPrimary
                font.pixelSize: Theme.titleSize
                font.bold: true
                font.family: Theme.fontFamily
            }
            Text {
                text: "离线质检工作站  v" + app.appVersion
                color: Theme.textSecondary
                font.pixelSize: 10
                font.family: Theme.fontFamily
            }
        }

        AppButton {
            text: "数据集"
            outlined: true
            enabled: !app.busy && !app.liveRunning
            onClicked: root.datasetClicked()
        }

        Item { Layout.fillWidth: true }

        EngineSwitch {
            engineKind: app.engineKind
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
                implicitWidth: 86
                onValueModified: app.liveTargetFps = value
            }
            AppButton {
                text: app.liveRunning ? "停止取流" : "开始取流"
                primary: !app.liveRunning
                highlight: app.liveRunning
                enabled: app.liveRunning || app.canStartLive
                onClicked: app.liveRunning ? app.stopLive() : app.startLive()
            }
        }

        AppButton {
            id: exportBtn
            text: "导出"
            outlined: true
            enabled: !app.busy && !app.liveRunning && (app.hasImage || app.canExportBatch)
            onClicked: exportMenu.open()
        }

        AppButton {
            text: "关于"
            outlined: true
            onClicked: root.aboutClicked()
        }
    }

    Menu {
        id: exportMenu
        x: exportBtn.mapToItem(root, 0, exportBtn.height + 4).x
        y: exportBtn.mapToItem(root, 0, exportBtn.height + 4).y
        palette.window: Theme.bgElevated
        palette.text: Theme.textPrimary
        MenuItem {
            text: "导出当前图（PNG）"
            enabled: app.hasImage
            onTriggered: root.exportCurrentClicked()
        }
        MenuItem {
            text: "导出批量（PNG + CSV）"
            enabled: app.canExportBatch
            onTriggered: root.exportBatchClicked()
        }
    }
}
