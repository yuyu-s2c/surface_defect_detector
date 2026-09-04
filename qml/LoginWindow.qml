import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// 独立登录窗。登入后隐藏，主窗再 show；退出登录再跳回来。CLI 不走这条。
ApplicationWindow {
    id: root
    objectName: "loginWindow"
    title: "登录 — 表面缺陷检测工作站"
    width: 460
    height: 560
    minimumWidth: 420
    minimumHeight: 480
    visible: !auth.loggedIn
    color: Theme.bgApp
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

    Rectangle {
        anchors.fill: parent
        color: Theme.bgApp

        Rectangle {
            id: card
            width: Math.min(400, parent.width - 40)
            implicitHeight: col.implicitHeight + 48
            anchors.centerIn: parent
            radius: Theme.radius
            color: Theme.bgElevated
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: col
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 24
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
                    Keys.onReturnPressed: passField.forceActiveFocus()
                }
                Text {
                    text: "口令"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.smallSize
                    font.family: Theme.fontFamily
                }
                AppTextField {
                    id: passField
                    Layout.fillWidth: true
                    placeholderText: "口令"
                    echoMode: TextInput.Password
                    Keys.onReturnPressed: root.submit()
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
                    text: "首次启动已写入 admin / engineer / operator，初始口令与账号相同，登录后请改口令。"
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
