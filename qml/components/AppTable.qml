import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 表头与格子共用 TableView 列宽；数字右齐、名称左齐，内边距同一套。
ColumnLayout {
    id: root
    spacing: 0
    Layout.fillWidth: true
    Layout.fillHeight: true

    property alias model: view.model
    property var columnWeight: []
    property var columnAlign: []
    property var textColumns: [0]
    property var accentColumns: []
    property bool selectable: false
    property int selectedRow: -1

    signal rowClicked(int row)

    Connections {
        target: root.model
        function onModelReset() { root.selectedRow = -1 }
    }

    function alignOf(column) {
        if (columnAlign && column < columnAlign.length)
            return columnAlign[column]
        return column === 0 ? Text.AlignLeft : Text.AlignRight
    }

    function isTextCol(column) {
        return textColumns && textColumns.indexOf(column) >= 0
    }

    function isAccentCol(column) {
        return accentColumns && accentColumns.indexOf(column) >= 0
    }

    function padLeft(column) {
        return alignOf(column) === Text.AlignRight ? 6 : Theme.tablePad
    }

    function padRight(column) {
        return alignOf(column) === Text.AlignRight ? Theme.tablePad : 6
    }

    function widthsFor(total, count) {
        const out = []
        if (count <= 0 || total <= 0)
            return out
        let sum = 0
        for (let i = 0; i < count; ++i)
            sum += (columnWeight && i < columnWeight.length) ? columnWeight[i] : 1
        if (sum <= 0)
            sum = count
        let used = 0
        for (let i = 0; i < count; ++i) {
            const w = (i === count - 1)
                      ? Math.max(0, total - used)
                      : Math.floor(total * ((columnWeight && i < columnWeight.length) ? columnWeight[i] : 1) / sum)
            out.push(w)
            used += w
        }
        return out
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: Theme.tableHeaderH
        color: Theme.bgElevated

        HorizontalHeaderView {
            id: header
            anchors.fill: parent
            syncView: view
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            delegate: Item {
                required property int column
                required property var model
                implicitHeight: Theme.tableHeaderH
                implicitWidth: 48
                Text {
                    anchors.fill: parent
                    leftPadding: root.padLeft(column)
                    rightPadding: root.padRight(column)
                    text: model.display ?? ""
                    color: Theme.textSecondary
                    font.pixelSize: Theme.smallSize
                    font.family: Theme.fontFamily
                    font.bold: true
                    horizontalAlignment: root.alignOf(column)
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }
    }

    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

    TableView {
        id: view
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        columnSpacing: 0
        rowSpacing: 0
        alternatingRows: false
        rowHeightProvider: function() { return Theme.tableRowH }
        columnWidthProvider: function(column) {
            const n = columns
            const w = Math.floor(width)
            const ws = root.widthsFor(w, n)
            return (column >= 0 && column < ws.length) ? ws[column] : 48
        }
        onWidthChanged: Qt.callLater(forceLayout)
        onColumnsChanged: Qt.callLater(forceLayout)

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: 6
                radius: 3
                color: Theme.border
                visible: parent.size < 1.0
            }
        }

        delegate: Rectangle {
            required property var model
            required property int row
            required property int column
            implicitHeight: Theme.tableRowH
            readonly property bool summary: model.isSummary === true
            color: {
                if (root.selectable && row === root.selectedRow)
                    return Theme.bgHover
                if (summary)
                    return Theme.accentDim
                return row % 2 ? "transparent" : Theme.tableStripe
            }

            Text {
                anchors.fill: parent
                leftPadding: root.padLeft(column)
                rightPadding: root.padRight(column)
                text: String(model.display ?? "")
                color: (!summary && root.isAccentCol(column)) ? Theme.accent : Theme.textPrimary
                font.pixelSize: Theme.smallSize
                font.family: root.isTextCol(column) ? Theme.fontFamily : Theme.monoFamily
                font.bold: summary
                horizontalAlignment: root.alignOf(column)
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            TapHandler {
                enabled: root.selectable
                onTapped: {
                    root.selectedRow = (root.selectedRow === row) ? -1 : row
                    root.rowClicked(row)
                }
            }
        }
    }
}
