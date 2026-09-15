import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: popup

    required property var host
    required property var libraryModel
    required property var nicknameController

    property int gameRow: -1
    property string gameAppId: ""
    property var info: ({})

    width: Math.min(620, host.width - 60)
    x: Math.round((host.width - width) / 2)
    y: Math.round((host.height - height) / 2)
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function findRow(appId) {
        for (let row = 0; row < libraryModel.count; ++row) {
            if (libraryModel.gameAt(row).appId === appId)
                return row
        }
        return -1
    }

    function reload() {
        const row = findRow(gameAppId)
        gameRow = row
        info = row >= 0 ? libraryModel.duplicateInfo(row) : ({})
    }

    function openFor(row) {
        const g = libraryModel.gameAt(row)
        gameAppId = g.appId || ""
        gameRow = row
        info = libraryModel.duplicateInfo(row)
        popup.open()
    }

    background: Rectangle {
        color: popup.host.controlBg
        radius: 10
        border.width: 1
        border.color: popup.host.dividerColor
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            text: "Duplicate installs"
            font.pixelSize: 20
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            text: "Link installs only when they are the same game. Reno119 keeps each executable, prefix, component state, notes, verification, and recovery data separate."
            wrapMode: Text.Wrap
            opacity: 0.62
            font.pixelSize: 11
        }
        Label {
            Layout.fillWidth: true
            text: (popup.info.name || "Game") + " · " + (popup.info.source || "")
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            visible: ((popup.info.linked || []).length > 0)
            text: "Linked alternate installs"
            font.bold: true
            opacity: 0.82
        }
        Repeater {
            model: popup.info.linked || []
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: linkedColumn.implicitHeight + 14
                radius: 6
                color: popup.host.alternateBg
                border.width: 1
                border.color: popup.host.dividerColor
                ColumnLayout {
                    id: linkedColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 7
                    spacing: 2
                    Label {
                        Layout.fillWidth: true
                        text: modelData.name || "Unknown game"
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: (modelData.source || "") + ((modelData.exePath || "").length > 0 ? " · " + modelData.exePath : "")
                        opacity: 0.58
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }
                }
            }
        }

        Label {
            visible: ((popup.info.suggestions || []).length > 0)
            text: "Probable duplicates"
            font.bold: true
            opacity: 0.82
        }
        Repeater {
            model: popup.info.suggestions || []
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: suggestionRow.implicitHeight + 14
                radius: 6
                color: popup.host.alternateBg
                border.width: 1
                border.color: popup.host.dividerColor
                RowLayout {
                    id: suggestionRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 7
                    spacing: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: modelData.name || "Unknown game"
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: (modelData.source || "") + ((modelData.exePath || "").length > 0 ? " · " + modelData.exePath : "")
                            opacity: 0.58
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }
                    }
                    RenoDialogButton {
                        theme: popup.host
                        text: "Link"
                        onClicked: {
                            if (popup.gameRow >= 0 && popup.libraryModel.linkAlternateInstall(popup.gameRow, modelData.appId || "")) {
                                popup.nicknameController.reselect(popup.gameAppId)
                                Qt.callLater(function() { popup.reload() })
                            }
                        }
                    }
                }
            }
        }

        Label {
            text: "Manual link"
            font.bold: true
            opacity: 0.82
        }
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: manualAlternateChoice
                Layout.fillWidth: true
                model: popup.info.others || []
                textRole: "label"
                enabled: count > 0
            }
            RenoDialogButton {
                theme: popup.host
                text: "Link selected"
                enabled: manualAlternateChoice.count > 0 && manualAlternateChoice.currentIndex >= 0
                onClicked: {
                    const entries = popup.info.others || []
                    const entry = entries[manualAlternateChoice.currentIndex] || ({})
                    if (popup.gameRow >= 0 && (entry.appId || "").length > 0 && popup.libraryModel.linkAlternateInstall(popup.gameRow, entry.appId)) {
                        popup.nicknameController.reselect(popup.gameAppId)
                        Qt.callLater(function() { popup.reload() })
                    }
                }
            }
        }
        Label {
            visible: ((popup.info.linked || []).length === 0) && ((popup.info.suggestions || []).length === 0)
            Layout.fillWidth: true
            text: "No probable duplicate installs are currently detected. You can still link another entry manually when launcher titles differ."
            wrapMode: Text.Wrap
            opacity: 0.62
        }

        RowLayout {
            Layout.fillWidth: true
            RenoDialogButton {
                theme: popup.host
                visible: ((popup.info.linked || []).length > 0)
                text: "Unlink this install"
                onClicked: {
                    if (popup.gameRow >= 0 && popup.libraryModel.unlinkAlternateInstall(popup.gameRow)) {
                        popup.nicknameController.reselect(popup.gameAppId)
                        Qt.callLater(function() { popup.reload() })
                    }
                }
            }
            Item { Layout.fillWidth: true }
            RenoDialogButton {
                theme: popup.host
                text: "Close"
                onClicked: popup.close()
            }
        }
    }
}
