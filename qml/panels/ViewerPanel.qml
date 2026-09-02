import QtQuick
import SurfaceDefect

Rectangle {
    id: root
    color: Theme.bgCanvas

    function fitCanvas() { canvas.fitView() }

    InspectionCanvas {
        id: canvas
        anchors.fill: parent
        image: app.sourceImage
        gtOverlay: app.gtOverlayImage
        detOverlay: app.detOverlayImage
        boxes: app.boxRects
        gtVisible: app.gtOverlayVisible
        detVisible: app.detOverlayVisible
    }

    EmptyState {
        anchors.fill: parent
        visible: !app.hasImage && !app.busy && !app.liveRunning && app.stationAlert.length === 0
        title: app.hasDataset ? "从左侧选择一张测试图" : "先打开数据集根目录"
        subtitle: app.hasDataset
                  ? "滚轮缩放，左键拖拽平移。红 = GT 标注，绿 = 当前引擎检出。空格开始模拟取流。"
                  : "顶栏「数据集」或 Ctrl+O。目录里放各类的 train/good 与 test/。"
    }

    Rectangle {
        visible: app.liveRunning
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 12
        implicitWidth: liveLock.implicitWidth + 20
        implicitHeight: 28
        radius: 14
        color: Theme.dangerDim
        border.color: Theme.danger
        Text {
            id: liveLock
            anchors.centerIn: parent
            text: "取流中 · 不可选图 / 切引擎 / 批量"
            color: Theme.danger
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            font.bold: true
        }
    }

    StationBanner {
        id: banner
        visible: !app.liveRunning && app.stationAlert.length > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        anchors.topMargin: app.hasImage ? 48 : 12
        message: app.stationAlert
        isError: app.stationAlertIsError
        pathHint: (app.engineKind === 1 && app.expectedOnnxPath.length > 0) ? app.expectedOnnxPath : ""
    }

    OverlayChipBar {
        visible: app.hasImage
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 12
        gtVisible: app.gtOverlayVisible
        detVisible: app.detOverlayVisible
        zoom: canvas.zoom
        onFitRequested: canvas.fitView()
        onGtToggled: (on) => { app.gtOverlayVisible = on }
        onDetToggled: (on) => { app.detOverlayVisible = on }
    }

    VerdictBadge {
        visible: app.hasImage
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        hasImage: app.hasImage
        detected: app.detected
        verdictOk: app.verdictOk
        liveRunning: app.liveRunning
        imageScore: app.imageScore
        imageThreshold: app.imageThreshold
    }

    LiveHud {
        visible: app.liveRunning
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 12
        latencyMs: app.liveLatencyMs
        queueDepth: app.liveQueueDepth
        queueMax: app.liveQueueMax
        actualFps: app.liveActualFps
        okCount: app.liveOkCount
        ngCount: app.liveNgCount
        lastNg: app.liveLastNg
    }

    Rectangle {
        id: ngFlash
        anchors.fill: parent
        color: "transparent"
        border.color: Theme.danger
        border.width: 0
        opacity: 0
        visible: app.liveRunning

        SequentialAnimation on opacity {
            id: flashAnim
            running: false
            NumberAnimation { to: 0.9; duration: 60 }
            PauseAnimation { duration: 80 }
            NumberAnimation { to: 0; duration: 280 }
        }
        NumberAnimation on border.width {
            id: flashBorder
            running: false
            from: 8
            to: 0
            duration: 420
        }
    }

    Connections {
        target: app
        function onLiveStatsChanged() {
            if (app.liveRunning && app.liveLastNg) {
                flashAnim.restart()
                flashBorder.restart()
            }
        }
    }

    BusyOverlay {
        anchors.fill: parent
        active: app.busy && !app.liveRunning
        text: app.progressText
        subtitle: app.busySubtitle
        current: app.progressCurrent
        total: app.progressTotal
        calibrating: app.busyKind === "calib"
    }
}
