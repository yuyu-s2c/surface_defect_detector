import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: win
    objectName: "mainWindow"
    // 几何由 main.cpp placeMainWindow 按可用桌面（含标题栏）夹紧并居中；
    // 这里只给最小可操作尺寸，避免 1400×900 在笔记本缩放下顶出屏幕。
    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 560
    visible: auth.loggedIn
    title: "表面缺陷检测工作站"
    color: Theme.bgApp
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodySize

    onClosing: Qt.quit()

    onVisibleChanged: {
        if (visible) {
            raise()
            requestActivate()
        }
    }

    function isTyping() {
        const item = win.activeFocusItem
        return item && (item instanceof TextInput || item instanceof TextEdit || item instanceof TextField)
    }

    header: AppHeader {
        id: header
        onExportCurrentClicked: saveDialog.open()
        onExportBatchClicked: folderDialog.open()
        onExportLiveClicked: liveFolderDialog.open()
        onDatasetClicked: datasetDialog.open()
        onAboutClicked: aboutDialog.open()
        onPasswordClicked: passwordDialog.open()
        onUsersClicked: usersDialog.open()
        onLogoutClicked: auth.logout()
    }

    footer: AppStatusBar {
        statusText: app.statusText
        tone: app.statusTone
        busy: app.busy
        current: app.progressCurrent
        total: app.progressTotal
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal
        handle: Rectangle {
            implicitWidth: 1
            color: Theme.border
        }

        DatasetPanel {
            SplitView.preferredWidth: 200
            SplitView.minimumWidth: 160
            SplitView.maximumWidth: 280
            onOpenDatasetRequested: datasetDialog.open()
        }

        ViewerPanel {
            id: viewer
            SplitView.fillWidth: true
            SplitView.minimumWidth: 420
        }

        Item {
            SplitView.preferredWidth: app.workMode === 0 ? 280 : 320
            SplitView.minimumWidth: app.workMode === 0 ? 240 : 280
            SplitView.maximumWidth: app.workMode === 0 ? 360 : 480

            ResultRail {
                anchors.fill: parent
                visible: app.workMode === 0
                onExportCurrentClicked: saveDialog.open()
                onExportLiveClicked: liveFolderDialog.open()
            }

            InspectorPanel {
                anchors.fill: parent
                visible: app.workMode === 1
            }
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
        id: liveFolderDialog
        currentFolder: app.suggestedLiveExportFolderUrl()
        onAccepted: app.exportLiveSession(selectedFolder)
    }

    FolderDialog {
        id: datasetDialog
        title: "请选择数据集根目录（含 metal_nut、screw 等类别目录）"
        currentFolder: app.datasetRootUrl()
        onAccepted: app.loadDataset(selectedFolder)
    }

    AppDialog {
        id: errorDialog
        title: "需要处理"
        standardButtons: Dialog.Ok
        width: 480
        Label {
            text: app.errorMessage
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
            width: parent.width
        }
    }

    AppDialog {
        id: aboutDialog
        title: "关于本工作站"
        standardButtons: Dialog.Ok
        width: 520
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

    SelfCheckDialog {
        id: selfCheckDialog
    }

    ChangePasswordDialog {
        id: passwordDialog
    }

    UserManageDialog {
        id: usersDialog
    }

    Connections {
        target: app
        function onErrorMessageChanged() {
            if (app.errorMessage.length > 0)
                errorDialog.open()
        }
        function onSelfCheckRequested() {
            selfCheckDialog.open()
        }
    }

    Connections {
        target: auth
        function onSessionChanged() {
            app.onAuthChanged(auth.loggedIn)
            if (auth.loggedIn) {
                app.operatorName = auth.displayName
                if (!app.hasDataset)
                    datasetDialog.open()
            } else {
                passwordDialog.close()
                usersDialog.close()
            }
        }
    }

    Shortcut { sequence: "Space"; enabled: auth.loggedIn && !win.isTyping(); onActivated: app.requestStartLive() }
    Shortcut { sequence: "Esc"; enabled: auth.loggedIn; onActivated: app.stopLive() }
    Shortcut { sequence: "B"; enabled: auth.loggedIn && auth.canAnalyze && !win.isTyping(); onActivated: app.runBatch() }
    Shortcut { sequence: "Shift+C"; enabled: auth.loggedIn && auth.canAnalyze && !win.isTyping(); onActivated: app.compareEngines() }
    Shortcut { sequence: "I"; enabled: auth.loggedIn && !win.isTyping() && !app.liveRunning && !app.busy; onActivated: app.workMode = 0 }
    Shortcut { sequence: "A"; enabled: auth.loggedIn && auth.canAnalyze && !win.isTyping() && !app.liveRunning && !app.busy; onActivated: app.workMode = 1 }
    Shortcut { sequence: "1"; enabled: auth.loggedIn && auth.canChangeEngine && !win.isTyping() && !app.liveRunning && !app.busy; onActivated: app.engineKind = 0 }
    Shortcut { sequence: "2"; enabled: auth.loggedIn && auth.canChangeEngine && !win.isTyping() && !app.liveRunning && !app.busy; onActivated: app.engineKind = 1 }
    Shortcut { sequence: "G"; enabled: auth.loggedIn && !win.isTyping(); onActivated: app.gtOverlayVisible = !app.gtOverlayVisible }
    Shortcut { sequence: "D"; enabled: auth.loggedIn && !win.isTyping(); onActivated: app.detOverlayVisible = !app.detOverlayVisible }
    Shortcut { sequence: "F"; enabled: auth.loggedIn && !win.isTyping(); onActivated: viewer.fitCanvas() }
    Shortcut { sequence: "Ctrl+O"; enabled: auth.loggedIn && auth.canChangeDataset; onActivated: datasetDialog.open() }
    Shortcut { sequence: "Ctrl+E"; enabled: auth.loggedIn; onActivated: { if (app.hasImage && !app.liveRunning && !app.busy) saveDialog.open() } }
    Shortcut { sequence: "Ctrl+Shift+E"; enabled: auth.loggedIn && auth.canAnalyze; onActivated: { if (app.canExportBatch && !app.liveRunning && !app.busy) folderDialog.open() } }
    Shortcut { sequence: "F1"; enabled: auth.loggedIn; onActivated: aboutDialog.open() }
}
