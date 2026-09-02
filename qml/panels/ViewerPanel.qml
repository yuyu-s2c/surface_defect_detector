import QtQuick
import SurfaceDefect

Rectangle {
    id: root
    color: Theme.bgCanvas

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
        visible: !app.hasImage && !app.busy && !app.liveRunning
        title: "从左侧选择一张测试图"
        subtitle: "滚轮缩放，左键拖拽平移。红 = GT 标注，绿 = 当前引擎检出。"
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
    }

    BusyOverlay {
        anchors.fill: parent
        active: app.busy
        text: app.progressText
        current: app.progressCurrent
        total: app.progressTotal
    }
}
