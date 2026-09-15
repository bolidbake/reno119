import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: control
    required property var host
    property string previewText: ""
    property string acceptText: "Install anyway"
    property string rejectText: "Cancel"
    property int dialogWidth: 620
    property int previewHeight: 440

    modal: true
    standardButtons: Dialog.NoButton
    x: Math.round((host.width - width) / 2)
    y: Math.round((host.height - height) / 2)
    width: Math.min(dialogWidth, host.width - 60)

    contentItem: ScrollView {
        implicitHeight: Math.min(control.previewHeight, previewLabel.implicitHeight + 24)
        contentWidth: availableWidth
        Label {
            id: previewLabel
            width: parent.width
            wrapMode: Text.Wrap
            text: control.previewText
        }
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
