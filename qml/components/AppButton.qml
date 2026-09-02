import QtQuick
import QtQuick.Controls

Button {
    id: root
    property bool primary: false
    property bool outlined: false
    property bool highlight: false
    hoverEnabled: true
    leftPadding: 14
    rightPadding: 14
    topPadding: 7
    bottomPadding: 7
    font.pixelSize: Theme.bodySize
    font.family: Theme.fontFamily

    background: Rectangle {
        radius: Theme.radiusSmall
        color: {
            if (!root.enabled)
                return Theme.bgElevated
            if (root.primary)
                return root.down ? Qt.darker(Theme.accent, 1.15)
                                 : (root.hovered ? Qt.lighter(Theme.accent, 1.12) : Theme.accent)
            if (root.down)
                return Theme.bgHover
            if (root.hovered)
                return Theme.bgHover
            return root.outlined ? "transparent" : Theme.bgElevated
        }
        border.width: (root.primary && root.enabled) ? 0 : 1
        border.color: root.highlight ? Theme.warn : Theme.border
    }

    contentItem: Text {
        text: root.text
        font: root.font
        color: {
            if (!root.enabled)
                return Theme.textSecondary
            if (root.primary)
                return Theme.bgApp
            return Theme.textPrimary
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        hoverEnabled: true
    }
}
