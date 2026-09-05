import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

AppDialog {
    id: root
    title: "修改密码"
    width: 400
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    ColumnLayout {
        width: parent.width
        spacing: 10

        Text {
            text: "当前密码"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }
        AppTextField {
            id: oldField
            Layout.fillWidth: true
            echoMode: TextInput.Password
        }
        Text {
            text: "新密码"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }
        AppTextField {
            id: newField
            Layout.fillWidth: true
            echoMode: TextInput.Password
        }
        Text {
            text: "确认新密码"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }
        AppTextField {
            id: confirmField
            Layout.fillWidth: true
            echoMode: TextInput.Password
            Keys.onReturnPressed: root.submit()
        }
        Text {
            visible: hint.length > 0
            Layout.fillWidth: true
            text: hint
            color: hintOk ? Theme.accent : Theme.danger
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
                text: "保存"
                primary: true
                onClicked: root.submit()
            }
        }
    }

    property string hint: ""
    property bool hintOk: false

    function submit() {
        if (newField.text !== confirmField.text) {
            hintOk = false
            hint = "两次新密码不一致"
            return
        }
        if (auth.changeOwnPassword(oldField.text, newField.text)) {
            hintOk = true
            hint = auth.lastMessage
            oldField.text = ""
            newField.text = ""
            confirmField.text = ""
        } else {
            hintOk = false
            hint = auth.lastError
        }
    }

    onOpened: {
        hint = ""
        oldField.text = ""
        newField.text = ""
        confirmField.text = ""
        oldField.forceActiveFocus()
    }
}
