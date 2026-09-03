import QtQuick
import QtQuick.Layouts

ColumnLayout {
    spacing: 0
    Layout.minimumHeight: 88

    AppTable {
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: app.boxModel
        columnWeight: [0.75, 1, 1, 1, 1, 1.15]
        columnAlign: [Text.AlignRight, Text.AlignRight, Text.AlignRight,
                      Text.AlignRight, Text.AlignRight, Text.AlignRight]
        textColumns: []
        visible: app.defectCount > 0
    }

    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: app.defectCount === 0
        title: app.hasImage ? "未检出缺陷框" : "未选择图片"
        subtitle: ""
    }
}
