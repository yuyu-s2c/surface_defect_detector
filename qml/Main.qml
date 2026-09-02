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
    title: "表面缺陷检测工具"
    color: Theme.bgApp
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodySize

    header: AppHeader {
        id: header
        onBatchClicked: app.runBatch()
        onCompareClicked: app.compareEngines()
        onExportCurrentClicked: saveDialog.open()
        onExportBatchClicked: folderDialog.open()
    }

    footer: AppStatusBar {
        statusText: app.statusText
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
            SplitView.preferredWidth: 240
            SplitView.minimumWidth: 200
            SplitView.maximumWidth: 360
        }

        ViewerPanel {
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
        onAccepted: app.loadDataset(selectedFolder)
    }

    Dialog {
        id: errorDialog
        modal: true
        anchors.centerIn: parent
        title: "提示"
        standardButtons: Dialog.Ok
        width: 420
        palette.window: Theme.bgElevated
        palette.windowText: Theme.textPrimary
        Label {
            text: app.errorMessage
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
            width: parent.width
        }
    }

    Connections {
        target: app
        function onErrorMessageChanged() {
            if (app.errorMessage.length > 0)
                errorDialog.open()
        }
    }

    Component.onCompleted: {
        if (!app.hasDataset)
            datasetDialog.open()
    }
}
