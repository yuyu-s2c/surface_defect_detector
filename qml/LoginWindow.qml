import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// 独立登录窗。登入后隐藏，主窗再 show；退出登录再跳回来。CLI 不走这条。
ApplicationWindow {
    id: root
    objectName: "loginWindow"
    title: "登录 — 表面缺陷检测工作站"
    width: 400
    height: Math.max(360, col.implicitHeight + 56)
    minimumWidth: 400
    maximumWidth: 400
    minimumHeight: 360
    visible: !auth.loggedIn
    color: Theme.bgElevated
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodySize

    // 主窗由 C++ 另 load，不要挂在这里：子 Window 会跟登录窗一起藏掉，看起来像闪退。
    onClosing: (close) => {
        if (auth.loggedIn)
            close.accepted = false
        else
            Qt.quit()
    }

    onVisibleChanged: {
        if (visible) {
            passField.text = ""
            raise()
            requestActivate()
            userField.forceActiveFocus()
        }
    }

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 28
        spacing: 12

        Text {
            text: "表面缺陷检测工作站"
            color: Theme.textPrimary
            font.pixelSize: Theme.titleSize
            font.bold: true
            font.family: Theme.fontFamily
            Layout.fillWidth: true
        }
        Text {
            text: "本机登录。操作员只跑检测台；工艺员可分析；管理员管账号。"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: "账号"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }
        AppTextField {
            id: userField
            Layout.fillWidth: true
            placeholderText: "用户名"
            onAccepted: {
                if (passField.text.length === 0)
                    passField.forceActiveFocus()
                else
                    root.submit()
            }
        }
        Text {
            text: "密码"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }
        AppTextField {
            id: passField
            Layout.fillWidth: true
            placeholderText: "密码"
            echoMode: TextInput.Password
            onAccepted: root.submit()
        }

        Text {
            visible: auth.lastError.length > 0
            Layout.fillWidth: true
            text: auth.lastError
            color: Theme.danger
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }
        Text {
            visible: auth.seededThisRun
            Layout.fillWidth: true
            text: "首次启动已写入 admin / engineer / operator，初始密码均为 123456，登录后请改密码。"
            color: Theme.warn
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }

        AppButton {
            Layout.fillWidth: true
            text: "登录"
            primary: true
            onClicked: root.submit()
        }
    }

    function submit() {
        if (auth.login(userField.text, passField.text))
            passField.text = ""
    }

    Component.onCompleted: {
        if (visible)
            userField.forceActiveFocus()
    }
}
