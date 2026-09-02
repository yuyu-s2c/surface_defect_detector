import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

ColumnLayout {
    spacing: 10

    component ParamRow: RowLayout {
        property string label: ""
        Layout.fillWidth: true
        spacing: 8
        Text {
            text: label
            color: Theme.textSecondary
            font.pixelSize: Theme.bodySize
            font.family: Theme.fontFamily
            Layout.preferredWidth: 92
        }
    }

    ColumnLayout {
        visible: app.engineKind !== 1
        spacing: 8
        Layout.fillWidth: true
        ParamRow {
            label: "聚合阈值"
            AppDoubleSpin {
                Layout.fillWidth: true
                realFrom: 0.1; realTo: 10; realStep: 0.1; decimals: 2
                realValue: app.cvZAggThreshold
                onEdited: (v) => { app.cvZAggThreshold = v }
            }
        }
        ParamRow {
            label: "闭运算核 px"
            AppSpinBox {
                Layout.fillWidth: true
                from: 1; to: 51; value: app.cvMorphCloseKernel
                onValueModified: app.cvMorphCloseKernel = value
            }
        }
        ParamRow {
            label: "最小面积"
            AppSpinBox {
                Layout.fillWidth: true
                from: 0; to: 100000; stepSize: 50; value: app.cvMinDefectArea
                onValueModified: app.cvMinDefectArea = value
            }
        }
        ParamRow {
            label: "图像级门"
            AppSpinBox {
                Layout.fillWidth: true
                from: 0; to: 1000000; stepSize: 100; value: app.cvImageLevelMinArea
                onValueModified: app.cvImageLevelMinArea = value
            }
        }
    }

    ColumnLayout {
        visible: app.engineKind === 1
        spacing: 8
        Layout.fillWidth: true
        ParamRow {
            label: "阈值 kσ"
            AppDoubleSpin {
                Layout.fillWidth: true
                realFrom: 0.1; realTo: 8; realStep: 0.1; decimals: 2
                realValue: app.dlThresholdSigma
                onEdited: (v) => { app.dlThresholdSigma = v }
            }
        }
        ParamRow {
            label: "闭运算核 px"
            AppSpinBox {
                Layout.fillWidth: true
                from: 1; to: 51; value: app.dlMorphCloseKernel
                onValueModified: app.dlMorphCloseKernel = value
            }
        }
        ParamRow {
            label: "最小面积"
            AppSpinBox {
                Layout.fillWidth: true
                from: 0; to: 100000; stepSize: 50; value: app.dlMinDefectArea
                onValueModified: app.dlMinDefectArea = value
            }
        }
        ParamRow {
            label: "叠加面积门"
            AppSpinBox {
                Layout.fillWidth: true
                from: 0; to: 1000000; stepSize: 50; value: app.dlImageLevelMinArea
                onValueModified: app.dlImageLevelMinArea = value
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        AppButton {
            Layout.fillWidth: true
            text: "应用"
            primary: app.paramsDirty
            highlight: app.paramsDirty
            enabled: !app.busy && !app.liveRunning && app.currentCategory.length > 0
            onClicked: app.applyParams()
        }
        AppButton {
            Layout.fillWidth: true
            text: "恢复默认"
            outlined: true
            enabled: !app.busy && !app.liveRunning && app.currentCategory.length > 0
            onClicked: app.restoreParams()
        }
    }

    Text {
        visible: app.paramsDirty
        text: "已改参数，点应用后才会重检"
        color: Theme.warn
        font.pixelSize: Theme.smallSize
        font.family: Theme.fontFamily
    }
}
