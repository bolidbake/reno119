import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var host
    required property var installerBackend
    required property var gameModelBackend
    property var gameIds: []
    property var items: []
    property bool retry: false
    property string errorText: ""

    title: retry ? "Review failed updates to retry" : "Review updates"
    modal: true
    width: Math.min(700, host.width - 60)
    height: Math.min(500, host.height - 80)
    x: Math.round((host.width - width) / 2)
    y: Math.round((host.height - height) / 2)

    contentItem: ColumnLayout {
        Label {
            Layout.fillWidth: true
            text: "Each component receives a pre-update recovery snapshot."
            wrapMode: Text.Wrap
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                Repeater {
                    model: dialog.items
                    delegate: Label {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.name + " · " + modelData.component + "\n" +
                              (modelData.installed || "unknown") + " → " + (modelData.available || "unknown") +
                              (modelData.matchRequiresConfirmation === true
                                ? "\n⚠ RenoDX catalog match: " + (modelData.matchTitle || "unknown title") + " · " + (modelData.matchMethod || "non-exact") + " — starting updates confirms this match."
                                : "")
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            visible: dialog.errorText.length > 0
            text: dialog.errorText
            color: dialog.host.warningColor
            wrapMode: Text.Wrap
        }
        RowLayout {
            Item { Layout.fillWidth: true }
            RenoDialogButton {
                theme: dialog.host
                text: "Cancel"
                onClicked: dialog.close()
            }
            RenoDialogButton {
                theme: dialog.host
                text: dialog.retry ? "Retry updates" : "Start updates"
                enabled: !dialog.installerBackend.busy
                         && !dialog.installerBackend.bulkUpdateBusy
                         && !dialog.installerBackend.updateQueueBusy
                         && !dialog.gameModelBackend.scanning
                         && dialog.items.length > 0
                onClicked: {
                    const current = dialog.host.updatePreviewItems(dialog.gameIds, dialog.retry)
                    if (JSON.stringify(current) !== JSON.stringify(dialog.items)) {
                        dialog.items = current
                        dialog.errorText = "Targets changed. Review the refreshed list before starting."
                        return
                    }
                    const rows = (dialog.host.updateCenterItems || []).filter(function(g) {
                        return dialog.gameIds.indexOf(g.appId) >= 0
                    }).map(function(g) { return g.row })
                    dialog.close()
                    if (dialog.retry) dialog.installerBackend.retryFailedUpdates(true)
                    else dialog.installerBackend.updateGames(rows, true)
                }
            }
        }
    }
}
