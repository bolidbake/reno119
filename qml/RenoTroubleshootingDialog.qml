import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var host
    required property var installerBackend
    property string rawReport: ""

    modal: true
    title: "Troubleshooting report"
    width: Math.min(900, host.width - 40)
    height: Math.min(720, host.height - 40)
    x: (host.width - width) / 2
    y: (host.height - height) / 2
    standardButtons: Dialog.Close

    contentItem: ColumnLayout {
        CheckBox {
            id: redactReport
            text: "Hide home-directory paths"
            checked: true
        }
        Label {
            Layout.fillWidth: true
            text: "Review before sharing. Per-game notes are excluded."
            wrapMode: Text.Wrap
        }
        ScrollView {
            id: reportScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            RenoTextArea {
                id: reportText
                width: reportScroll.availableWidth
                height: Math.max(reportScroll.availableHeight, implicitHeight)
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                font.family: "monospace"
                text: redactReport.checked ? dialog.installerBackend.redactDiagnostics(dialog.rawReport) : dialog.rawReport
                backgroundColor: dialog.host.controlBg
                foregroundColor: dialog.host.textColor
                borderColor: dialog.host.dividerColor
                placeholderColor: dialog.host.mutedTextColor
                selectionColor: dialog.host.selectedBg
            }
        }
        RenoDialogButton {
            theme: dialog.host
            text: "Copy report"
            onClicked: dialog.installerBackend.copyText(reportText.text)
        }
    }
}
