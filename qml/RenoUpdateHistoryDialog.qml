import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var host
    required property var installerBackend
    property int gameRow: -1
    property string gameName: ""
    property var history: []
    signal rollbackRequested(int row, string path, string label)

    function openFor(row, name) {
        gameRow = row
        gameName = name
        history = installerBackend.updateHistory(row)
        open()
    }

    title: "Update history — " + gameName
    modal: true
    width: Math.min(680, host.width - 60)
    height: Math.min(520, host.height - 80)
    standardButtons: Dialog.Close
    x: Math.round((host.width - width) / 2)
    y: Math.round((host.height - height) / 2)

    contentItem: ColumnLayout {
        spacing: 8
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "Each Update Center operation creates a pre-update snapshot. Rollback restores that snapshot through Reno119's existing Recovery system."
            opacity: 0.62
            font.pixelSize: 11
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            ColumnLayout {
                width: Math.max(0, parent.width - 12)
                spacing: 7
                Label {
                    visible: (dialog.history || []).length === 0
                    text: "No Update Center rollback snapshots are available for this game yet."
                    opacity: 0.58
                    wrapMode: Text.Wrap
                }
                Repeater {
                    model: dialog.history || []
                    delegate: Rectangle {
                        required property var modelData
                        property var historyItem: modelData
                        Layout.fillWidth: true
                        implicitHeight: historyRow.implicitHeight + 12
                        radius: 6
                        color: dialog.host.controlBg
                        border.width: 1
                        border.color: dialog.host.dividerColor
                        RowLayout {
                            id: historyRow
                            anchors.fill: parent
                            anchors.margins: 6
                            ColumnLayout {
                                Layout.fillWidth: true
                                Label { text: historyItem.label || "Update snapshot"; font.bold: true }
                                Label { text: historyItem.createdDisplay || ""; opacity: 0.55; font.pixelSize: 10 }
                            }
                            RenoDialogButton {
                                theme: dialog.host
                                text: "Rollback"
                                onClicked: {
                                    const path = historyItem.path || ""
                                    const label = historyItem.label || "Update snapshot"
                                    dialog.close()
                                    dialog.rollbackRequested(dialog.gameRow, path, label)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
