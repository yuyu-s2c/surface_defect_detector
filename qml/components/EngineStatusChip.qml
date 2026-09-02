import QtQuick

Rectangle {
    id: root
    implicitHeight: 40
    implicitWidth: Math.min(col.implicitWidth + 16, 360)
    radius: Theme.radius
    color: Theme.bgElevated
    border.width: 1
    border.color: {
        if (app.engineKind === 1 && app.stationAlertIsError)
            return Theme.danger
        if (app.engineKind === 1 && app.stationAlert.length > 0)
            return Theme.warn
        return Theme.border
    }

    Column {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 0
        Text {
            width: parent.width
            text: app.engineStatusText
            color: Theme.textPrimary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            font.bold: true
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: app.providerText + " · " + app.calibStatusText
            color: Theme.textSecondary
            font.pixelSize: 10
            font.family: Theme.fontFamily
            elide: Text.ElideRight
        }
    }
}
