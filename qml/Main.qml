import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import SurfaceDefect

ApplicationWindow {
    id: win
    width: 1400
    height: 900
    minimumWidth: 1100
    minimumHeight: 700
    visible: true
    title: "表面缺陷检测工作站"
    color: Theme.bgApp
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodySize

    function isTyping() {
        const item = win.activeFocusItem
        return item && (item instanceof TextInput || item instanceof TextEdit || item instanceof TextField)
    }

    header: AppHeader {
        id: header
        onBatchClicked: app.runBatch()
        onCompareClicked: app.compareEngines()
        onExportCurrentClicked: saveDialog.open()
        onExportBatchClicked: folderDialog.open()
        onDatasetClicked: datasetDialog.open()
        onAboutClicked: aboutDialog.open()
    }

    footer: AppStatusBar {
        statusText: app.statusText
        tone: app.statusTone
        busy: app.busy
        current: app.progressCurrent
        total: app.progressTotal
        liveOk: app.liveOkCount
        liveNg: app.liveNgCount
        liveRunning: app.liveRunning
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal
        handle: Rectangle {
            implicitWidth: 1
            color: Theme.border
        }

        DatasetPanel {
            SplitView.preferredWidth: 240
            SplitView.minimumWidth: 200
            SplitView.maximumWidth: 360
            onOpenDatasetRequested: datasetDialog.open()
        }

        ViewerPanel {
            id: viewer
            SplitView.fillWidth: true
            SplitView.minimumWidth: 420
        }

        InspectorPanel {
            SplitView.preferredWidth: 380
            SplitView.minimumWidth: 320
            SplitView.maximumWidth: 480
            onExportCurrentClicked: saveDialog.open()
        }
    }

    ToastBanner {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        z: 20
        message: app.toastMessage
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG 图片 (*.png)"]
        defaultSuffix: "png"
        currentFile: app.suggestedExportFileUrl()
        onAccepted: app.exportCurrent(selectedFile)
    }

    FolderDialog {
        id: folderDialog
        currentFolder: app.suggestedExportFolderUrl()
        onAccepted: app.exportBatch(selectedFolder)
    }

    FolderDialog {
        id: datasetDialog
        title: "请选择数据集根目录（含 metal_nut、screw 等类别目录）"
        currentFolder: app.datasetRootUrl()
        onAccepted: app.loadDataset(selectedFolder)
    }

    Dialog {
        id: errorDialog
        modal: true
        anchors.centerIn: parent
        title: "需要处理"
        standardButtons: Dialog.Ok
        width: 480
        palette.window: Theme.bgElevated
        palette.windowText: Theme.textPrimary
        Label {
            text: app.errorMessage
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
            width: parent.width
        }
    }

    Dialog {
        id: aboutDialog
        modal: true
        anchors.centerIn: parent
        title: "关于本工作站"
        standardButtons: Dialog.Ok
        width: 520
        palette.window: Theme.bgElevated
        palette.windowText: Theme.textPrimary
        Column {
            width: parent.width
            spacing: 10
            Text {
                width: parent.width
                text: "表面缺陷检测工作站  v" + app.appVersion
                color: Theme.textPrimary
                font.pixelSize: Theme.titleSize
                font.bold: true
                font.family: Theme.fontFamily
            }
            Text {
                width: parent.width
                text: app.aboutBody
                wrapMode: Text.WordWrap
                color: Theme.textPrimary
                font.pixelSize: Theme.bodySize
                font.family: Theme.fontFamily
            }
            Text {
                width: parent.width
                text: app.scoreRuleText
                wrapMode: Text.WordWrap
                color: Theme.accent
                font.pixelSize: Theme.bodySize
                font.family: Theme.fontFamily
            }
            Text {
                width: parent.width
                text: app.shortcutsHelp
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.monoFamily
            }
        }
    }

    Connections {
        target: app
        function onErrorMessageChanged() {
            if (app.errorMessage.length > 0)
                errorDialog.open()
        }
    }

    Shortcut { sequence: "Space"; enabled: !win.isTyping(); onActivated: app.liveRunning ? app.stopLive() : app.startLive() }
    Shortcut { sequence: "Esc"; onActivated: app.stopLive() }
    Shortcut { sequence: "B"; enabled: !win.isTyping(); onActivated: app.runBatch() }
    Shortcut { sequence: "Shift+C"; enabled: !win.isTyping(); onActivated: app.compareEngines() }
    Shortcut { sequence: "1"; enabled: !win.isTyping() && !app.liveRunning && !app.busy; onActivated: app.engineKind = 0 }
    Shortcut { sequence: "2"; enabled: !win.isTyping() && !app.liveRunning && !app.busy; onActivated: app.engineKind = 1 }
    Shortcut { sequence: "G"; enabled: !win.isTyping(); onActivated: app.gtOverlayVisible = !app.gtOverlayVisible }
    Shortcut { sequence: "D"; enabled: !win.isTyping(); onActivated: app.detOverlayVisible = !app.detOverlayVisible }
    Shortcut { sequence: "F"; enabled: !win.isTyping(); onActivated: viewer.fitCanvas() }
    Shortcut { sequence: "Ctrl+O"; onActivated: datasetDialog.open() }
    Shortcut { sequence: "Ctrl+E"; onActivated: { if (app.hasImage && !app.liveRunning && !app.busy) saveDialog.open() } }
    Shortcut { sequence: "Ctrl+Shift+E"; onActivated: { if (app.canExportBatch && !app.liveRunning && !app.busy) folderDialog.open() } }
    Shortcut { sequence: "F1"; onActivated: aboutDialog.open() }

    Component.onCompleted: {
        if (!app.hasDataset)
            datasetDialog.open()
    }
}
