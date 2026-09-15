import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var host
    property string target: "opti"
    property var hotkeyOptions: []
    property alias currentIndex: hotkeyChoice.currentIndex
    readonly property var selectedItem: currentIndex >= 0 && currentIndex < hotkeyOptions.length
                                        ? hotkeyOptions[currentIndex]
                                        : null

    modal: true
    title: target === "opti" ? "Change OptiScaler overlay hotkey" : "Change REFramework overlay hotkey"
    standardButtons: Dialog.NoButton
    width: Math.min(430, host.width - 60)
    x: Math.round((host.width - width) / 2)
    y: Math.round((host.height - height) / 2)

    footer: RowLayout {
        spacing: 8
        Item { Layout.fillWidth: true }
        RenoDialogButton {
            theme: dialog.host
            text: "Cancel"
            onClicked: dialog.reject()
        }
        RenoDialogButton {
            theme: dialog.host
            text: "Apply"
            onClicked: dialog.accept()
        }
        Item { width: 8 }
    }

    ColumnLayout {
        width: parent ? parent.width : 390
        spacing: 10
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "Choose a Windows virtual-key for this overlay. Reno119 will only edit the overlay shortcut setting."
            opacity: 0.7
        }
        RenoComboBox {
            id: hotkeyChoice
            Layout.fillWidth: true
            model: dialog.hotkeyOptions
            textRole: "name"
        }
    }
}
