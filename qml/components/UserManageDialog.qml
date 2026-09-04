import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

AppDialog {
    id: root
    title: "用户管理"
    width: 560
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string selectedUser: ""
    property int formRole: 0

    ColumnLayout {
        width: parent.width
        spacing: 10

        AppTable {
            id: table
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            model: auth.userModel
            selectable: true
            columnWeight: [1.2, 1.2, 0.9, 0.7]
            columnAlign: [Text.AlignLeft, Text.AlignLeft, Text.AlignLeft, Text.AlignLeft]
            textColumns: [0, 1, 2, 3]
            onRowClicked: function(row) {
                if (table.selectedRow < 0) {
                    root.selectedUser = ""
                    return
                }
                root.selectedUser = auth.userModel.usernameAt(row)
                displayField.text = auth.userModel.displayNameAt(row)
                root.formRole = auth.userModel.roleAt(row)
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.selectedUser.length > 0
                  ? ("已选 " + root.selectedUser)
                  : "点一行再改角色 / 停用 / 重置口令 / 删除"
            color: Theme.textSecondary
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: form.implicitHeight + 20
            radius: Theme.radiusSmall
            color: Theme.bgApp
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: form
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 8

                Text {
                    text: "新增账号"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.smallSize
                    font.family: Theme.fontFamily
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    AppTextField {
                        id: userField
                        Layout.fillWidth: true
                        placeholderText: "账号"
                    }
                    AppTextField {
                        id: displayField
                        Layout.fillWidth: true
                        placeholderText: "显示名"
                    }
                }
                AppTextField {
                    id: passField
                    Layout.fillWidth: true
                    placeholderText: "口令（新增或重置）"
                    echoMode: TextInput.Password
                }

                Row {
                    spacing: 4
                    Repeater {
                        model: [
                            { label: "操作员", role: 0 },
                            { label: "工艺员", role: 1 },
                            { label: "管理员", role: 2 }
                        ]
                        delegate: Rectangle {
                            required property var modelData
                            width: 72
                            height: 28
                            radius: Theme.radiusSmall
                            color: root.formRole === modelData.role ? Theme.accent : Theme.bgElevated
                            border.width: 1
                            border.color: Theme.border
                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: root.formRole === modelData.role ? Theme.bgApp : Theme.textPrimary
                                font.pixelSize: Theme.smallSize
                                font.family: Theme.fontFamily
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.formRole = modelData.role
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    AppButton {
                        text: "新增"
                        primary: true
                        onClicked: {
                            if (auth.addUser(userField.text, displayField.text, passField.text, root.formRole)) {
                                userField.text = ""
                                passField.text = ""
                                table.selectedRow = -1
                                root.selectedUser = ""
                            }
                        }
                    }
                    AppButton {
                        text: "改显示名"
                        outlined: true
                        enabled: root.selectedUser.length > 0
                        onClicked: auth.setUserDisplayName(root.selectedUser, displayField.text)
                    }
                    AppButton {
                        text: "改角色"
                        outlined: true
                        enabled: root.selectedUser.length > 0
                        onClicked: auth.setUserRole(root.selectedUser, root.formRole)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    AppButton {
                        text: (table.selectedRow >= 0 && auth.userModel.enabledAt(table.selectedRow))
                              ? "停用" : "启用"
                        outlined: true
                        enabled: root.selectedUser.length > 0
                        onClicked: {
                            const on = table.selectedRow >= 0 && auth.userModel.enabledAt(table.selectedRow)
                            auth.setUserEnabled(root.selectedUser, !on)
                        }
                    }
                    AppButton {
                        text: "重置口令"
                        outlined: true
                        enabled: root.selectedUser.length > 0
                        onClicked: auth.resetPassword(root.selectedUser, passField.text)
                    }
                    AppButton {
                        text: "删除"
                        outlined: true
                        enabled: root.selectedUser.length > 0
                        onClicked: {
                            if (auth.removeUser(root.selectedUser)) {
                                root.selectedUser = ""
                                table.selectedRow = -1
                            }
                        }
                    }
                }
            }
        }

        Text {
            visible: auth.lastError.length > 0 || auth.lastMessage.length > 0
            Layout.fillWidth: true
            text: auth.lastError.length > 0 ? auth.lastError : auth.lastMessage
            color: auth.lastError.length > 0 ? Theme.danger : Theme.accent
            font.pixelSize: Theme.smallSize
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            AppButton {
                text: "关闭"
                outlined: true
                onClicked: root.close()
            }
        }
    }

    onOpened: {
        auth.clearError()
        root.selectedUser = ""
        table.selectedRow = -1
        userField.text = ""
        displayField.text = ""
        passField.text = ""
        root.formRole = 0
    }
}
