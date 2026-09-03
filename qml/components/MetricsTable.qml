import QtQuick
import QtQuick.Layouts

AppTable {
    model: app.metricsModel
    columnWeight: [1.35, 1, 1, 1, 1, 1.7]
    columnAlign: [Text.AlignLeft, Text.AlignRight, Text.AlignRight,
                  Text.AlignRight, Text.AlignRight, Text.AlignRight]
    textColumns: [0]
    accentColumns: [3]
}
