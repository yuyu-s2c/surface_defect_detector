import QtQuick
import QtQuick.Layouts

// 顶栏工位身份：相机离线 / FolderSource 模拟取流 / 模拟 PLC，避免被看成实机已接。

Rectangle {
    id: root
    color: Theme.bgElevated
    implicitHeight: 28

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        component Chip: Row {
            property string label: ""
            property string value: ""
            property color tone: Theme.textSecondary
            spacing: 6
            Rectangle {
                width: 7
                height: 7
                radius: 4
                anchors.verticalCenter: parent.verticalCenter
                color: tone
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: label + "  " + value
                color: Theme.textSecondary
                font.pixelSize: Theme.smallSize
                font.family: Theme.fontFamily
            }
        }

        Chip {
            label: "相机"
            value: app.cameraStatusText
            tone: Theme.warn
        }
        Chip {
            label: "取流"
            value: app.sourceStatusText
            tone: app.liveRunning ? Theme.accent : Theme.textSecondary
        }
        Chip {
            label: "剔除"
            value: app.plcStatusText
            tone: app.liveLastNg && app.liveRunning ? Theme.danger : Theme.textSecondary
        }

        Item { Layout.fillWidth: true }

        Text {
            visible: app.workOrder.length > 0
            text: "工单  " + app.workOrder
            color: Theme.textPrimary
            font.pixelSize: Theme.smallSize
            font.family: Theme.monoFamily
            elide: Text.ElideRight
            Layout.maximumWidth: 220
        }
        Text {
            visible: app.liveInterlocked
            text: "联锁停线"
            color: Theme.danger
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            font.bold: true
        }
    }
}
