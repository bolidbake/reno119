import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: popup
    required property var host
    required property var libraryModel
    required property var listView
    property int editRow: -1

    width: Math.min(460, host.width - 60)
    x: Math.round((host.width - width) / 2)
    y: Math.round((host.height - height) / 2)
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openFor(row) {
        const g = libraryModel.gameAt(row)
        editRow = row
        nicknameField.text = g.nickname || ""
        nicknameField.placeholderText = g.originalName || g.name || "Game nickname"
        popup.open()
        nicknameField.forceActiveFocus()
        nicknameField.selectAll()
    }

    function reselect(appId) {
        Qt.callLater(function() {
            for (let row = 0; row < libraryModel.count; ++row) {
                if (libraryModel.gameAt(row).appId === appId) {
                    listView.currentIndex = row
                    return
                }
            }
        })
    }

    function commit() {
        if (editRow < 0 || nicknameField.text.trim().length === 0)
            return
        const appId = libraryModel.gameAt(editRow).appId || ""
        if (libraryModel.setGameNickname(editRow, nicknameField.text)) {
            popup.close()
            popup.reselect(appId)
        }
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
            text: "Game nickname"
            font.pixelSize: 20
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            text: "Changes only how this game is displayed in Reno119. Launcher metadata and compatibility matching keep using the original title."
            wrapMode: Text.Wrap
            opacity: 0.62
            font.pixelSize: 11
        }
        TextField {
            id: nicknameField
            Layout.fillWidth: true
            onAccepted: popup.commit()
        }
        RowLayout {
            Item { Layout.fillWidth: true }
            RenoDialogButton {
                theme: popup.host
                text: "Cancel"
                onClicked: popup.close()
            }
            RenoDialogButton {
                theme: popup.host
                text: "Save"
                enabled: nicknameField.text.trim().length > 0
                onClicked: popup.commit()
            }
        }
    }
}
