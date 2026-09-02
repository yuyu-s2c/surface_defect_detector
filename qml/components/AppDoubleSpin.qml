import QtQuick
import QtQuick.Controls

Item {
    id: root
    property int decimals: 2
    property real realFrom: 0
    property real realTo: 10
    property real realStep: 0.1
    property real realValue: 0
    signal edited(real value)

    implicitWidth: 140
    implicitHeight: 30

    AppSpinBox {
        id: spin
        anchors.fill: parent
        from: 0
        to: Math.max(0, Math.round((root.realTo - root.realFrom) / root.realStep))
        stepSize: 1
        value: Math.round((root.realValue - root.realFrom) / root.realStep)
        textFromValue: function(v) {
            return (root.realFrom + v * root.realStep).toFixed(root.decimals)
        }
        valueFromText: function(text) {
            const n = Number(text)
            if (isNaN(n))
                return spin.value
            return Math.round((n - root.realFrom) / root.realStep)
        }
        onValueModified: root.edited(root.realFrom + spin.value * root.realStep)
    }

    onRealValueChanged: {
        const v = Math.round((root.realValue - root.realFrom) / root.realStep)
        if (spin.value !== v)
            spin.value = v
    }
}
