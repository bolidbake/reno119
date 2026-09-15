import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var host
    property int gameRow: -1
    property bool externalOverwrite: false
    property string gameName: ""
    property string matchTitle: ""
    property string matchMethod: ""
    property string addonFile: ""
    property string matchUrl: ""

    modal: true
    title: "Confirm RenoDX catalog match"
    standardButtons: Dialog.NoButton
    width: Math.min(620, host.width - 60)
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
            text: "Install this addon"
            onClicked: dialog.accept()
        }
        Item { width: 8 }
    }

    contentItem: ColumnLayout {
        spacing: 10
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "Reno119 did not get an exact full-title match for “" + dialog.gameName + "”. Review the catalog target before installing."
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "Catalog title: " + (dialog.matchTitle || "Unknown") +
                  "\nMatch method: " + (dialog.matchMethod || "Non-exact") +
                  (dialog.addonFile.length > 0 ? "\nAddon: " + dialog.addonFile : "")
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "Continue only if this catalog title is actually the same game. Cancel if the match belongs to another entry in the same series."
            color: dialog.host.warningColor
        }
    }
}
