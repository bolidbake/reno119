import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: control
    required property var host
    property string message: ""
    property string acceptText: "OK"
    property string rejectText: "Cancel"
    property int dialogWidth: 560

    modal: true
    standardButtons: Dialog.NoButton
    x: Math.round((host.width - width) / 2)
    y: Math.round((host.height - height) / 2)
    width: Math.min(dialogWidth, host.width - 60)

    contentItem: Label {
        width: parent ? parent.width : Math.max(320, control.dialogWidth - 60)
        wrapMode: Text.Wrap
        text: control.message
    }

    footer: RowLayout {
        spacing: 8
        Item { Layout.fillWidth: true }
        RenoDialogButton {
            theme: control.host
            text: control.rejectText
            onClicked: control.reject()
        }
        RenoDialogButton {
            theme: control.host
            text: control.acceptText
            onClicked: control.accept()
        }
        Item { width: 8 }
    }
}
