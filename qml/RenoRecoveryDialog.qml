import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var host
    required property var installerBackend
    property int gameRow: -1
    property string recoveryPath: ""
    property var review: ({})
    signal restoreCompleted()

    modal: true
    title: "Review recovery"
    width: Math.min(820, host.width - 40)
    height: Math.min(620, host.height - 40)
    x: (host.width - width) / 2
    y: (host.height - height) / 2
    standardButtons: Dialog.NoButton

    contentItem: ColumnLayout {
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: dialog.review.error || "Review the file replacements below before restoring."
        }
        ScrollView {
            id: recoveryReviewScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            RenoTextArea {
                width: recoveryReviewScroll.availableWidth
                height: Math.max(recoveryReviewScroll.availableHeight, implicitHeight)
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                text: dialog.review.text || ""
                backgroundColor: dialog.host.controlBg
                foregroundColor: dialog.host.textColor
                borderColor: dialog.host.dividerColor
                placeholderColor: dialog.host.mutedTextColor
                selectionColor: dialog.host.selectedBg
            }
        }
    }

    footer: RowLayout {
        Item { Layout.fillWidth: true }
        RenoDialogButton {
            theme: dialog.host
            text: "Cancel"
            onClicked: dialog.close()
        }
        RenoDialogButton {
            theme: dialog.host
            text: "Refresh preview"
            onClicked: dialog.review = dialog.installerBackend.recoveryPreview(dialog.gameRow, dialog.recoveryPath)
        }
        RenoDialogButton {
            theme: dialog.host
            text: "Restore this copy"
            enabled: !dialog.installerBackend.busy && dialog.review.canRestore === true
            onClicked: {
                dialog.installerBackend.restoreRecovery(dialog.gameRow, dialog.recoveryPath, dialog.review.token)
                dialog.restoreCompleted()
                dialog.close()
            }
        }
    }
}
