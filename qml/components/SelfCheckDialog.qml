import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 开线前自检。通过后才 confirmStartLive；图源由顶栏 liveSourceKind 决定。

Dialog {
    id: root
    modal: true
    title: "开线自检"
    width: 520
    anchors.centerIn: parent
    palette.window: Theme.bgElevated
    palette.windowText: Theme.textPrimary
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    ColumnLayout {
        width: parent.width
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: app.selfCheckIntroText
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: app.selfCheckItems
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: col.implicitHeight + 16
                radius: Theme.radiusSmall
                color: Theme.bgApp
                border.width: 1
                border.color: modelData.ok ? Theme.border : Theme.danger

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: modelData.ok ? Theme.accent : Theme.danger
                    }
                    Column {
                        id: col
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            width: parent.width
                            text: String(modelData.title)
                            color: Theme.textPrimary
                            font.pixelSize: Theme.bodySize
                            font.family: Theme.fontFamily
                            font.bold: true
                        }
                        Text {
                            width: parent.width
                            text: String(modelData.detail)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.smallSize
                            font.family: Theme.fontFamily
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: app.selfCheckHint
            color: app.selfCheckPassed ? Theme.accent : Theme.danger
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            AppButton {
                text: "取消"
                outlined: true
                onClicked: root.close()
            }
            AppButton {
                text: "确认开线"
                primary: true
                enabled: app.selfCheckPassed
                onClicked: {
                    app.confirmStartLive()
                    root.close()
                }
            }
        }
    }
}
