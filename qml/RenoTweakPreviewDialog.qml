import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var host
    required property var installerBackend
    property int gameRow: -1
    property bool restoring: false
    property var review: ({})

    modal: true
    title: restoring ? "Preview original-file restore" : "Preview tweak changes"
    x: Math.round((host.width - width) / 2)
    y: Math.round((host.height - height) / 2)
    width: Math.min(980, host.width - 40)
    height: Math.min(740, host.height - 40)
    standardButtons: Dialog.NoButton

    function reviewGame(row, restore) {
        gameRow = row
        restoring = restore
        refreshReview()
        open()
    }

    function refreshReview() {
        acknowledgeTweakChanges.checked = false
        review = installerBackend.previewRenoDxTweaks(gameRow, restoring)
    }

    contentItem: ColumnLayout {
        spacing: 10
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: dialog.restoring
                ? "Review the exact files that will be restored or removed. A recovery copy of the current files will be kept."
                : "Review the current values and proposed file contents before applying. Unrelated settings are retained."
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: dialog.review.warning || ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: dialog.host.warningColor
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: dialog.review.error || ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: dialog.host.warningColor
        }
        ScrollView {
            id: tweakPreviewScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            RenoTextArea {
                width: tweakPreviewScroll.availableWidth
                height: Math.max(tweakPreviewScroll.availableHeight, implicitHeight)
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                font.family: "monospace"
                text: dialog.review.text || "No preview available."
                backgroundColor: dialog.host.controlBg
                foregroundColor: dialog.host.textColor
                borderColor: dialog.host.dividerColor
                placeholderColor: dialog.host.mutedTextColor
                selectionColor: dialog.host.selectedBg
            }
        }
        CheckBox {
            id: acknowledgeTweakChanges
            Layout.fillWidth: true
            visible: dialog.review.requiresAcknowledgement === true
            text: "I reviewed the changes; keep a recovery copy and continue."
        }
    }

    footer: RowLayout {
        spacing: 8
        Item { Layout.fillWidth: true }
        RenoDialogButton {
            theme: dialog.host
            text: "Refresh preview"
            enabled: !dialog.installerBackend.busy
            onClicked: dialog.refreshReview()
        }
        RenoDialogButton {
            theme: dialog.host
            text: "Cancel"
            onClicked: dialog.reject()
        }
        RenoDialogButton {
            theme: dialog.host
            text: dialog.restoring ? "Restore original" : "Apply changes"
            enabled: !dialog.installerBackend.busy && dialog.review.canProceed === true
                     && (dialog.review.requiresAcknowledgement !== true || acknowledgeTweakChanges.checked)
            onClicked: {
                if (dialog.restoring)
                    dialog.installerBackend.restoreRenoDxTweaks(dialog.gameRow, dialog.review.token, acknowledgeTweakChanges.checked)
                else
                    dialog.installerBackend.applyRenoDxTweaks(dialog.gameRow, dialog.review.token, acknowledgeTweakChanges.checked)
                dialog.close()
            }
        }
        Item { width: 8 }
    }
}
