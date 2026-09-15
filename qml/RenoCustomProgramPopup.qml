import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: popup
    required property var host
    required property var libraryModel
    property bool editing: false
    property int editRow: -1

    signal browseExecutable(string currentPath)
    signal browsePrefix(string currentPath)
    signal browseArtwork(string currentPath)

    width: 520
    x: Math.round((host.width - width) / 2)
    y: Math.round((host.height - height) / 2)
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function setExecutablePath(path) {
        executableField.text = path || ""
    }

    function setPrefixPath(path) {
        prefixField.text = path || ""
    }

    function setArtworkPath(path) {
        artworkField.text = path || ""
    }

    function openAdd() {
        editing = false
        editRow = -1
        nameField.text = ""
        executableField.text = ""
        prefixField.text = ""
        artworkField.text = ""
        errorLabel.text = ""
        popup.open()
    }

    function openEdit(row) {
        const g = libraryModel.gameAt(row)
        editing = true
        editRow = row
        nameField.text = g.originalName || g.name || ""
        executableField.text = g.detectedExePath || g.exePath || ""
        prefixField.text = g.protonPrefix || ""
        artworkField.text = g.artworkOverridden === true ? (g.originalCoverArtPath || "") : (g.coverArtPath || "")
        errorLabel.text = ""
        popup.open()
    }

    function commit() {
        let ok = false
        if (editing) {
            ok = libraryModel.updateCustomProgram(editRow, nameField.text, executableField.text, prefixField.text, artworkField.text)
        } else {
            ok = libraryModel.addCustomProgram(nameField.text, executableField.text, prefixField.text, artworkField.text).length > 0
        }
        if (ok) {
            popup.close()
        } else {
            errorLabel.text = "Could not add this program. Check that the executable path exists and points to a valid Windows .exe."
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
            text: popup.editing ? "Edit custom program" : "Add custom program"
            font.pixelSize: 20
            font.bold: true
        }
        Label { text: "Display name"; opacity: 0.65 }
        TextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: "Program or game name"
        }
        Label { text: "Windows executable"; opacity: 0.65 }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: executableField
                Layout.fillWidth: true
                placeholderText: "/path/to/game.exe"
            }
            RenoDialogButton {
                theme: popup.host
                text: "Browse…"
                onClicked: popup.browseExecutable(executableField.text)
            }
        }
        Label { text: "Wine / Proton prefix (optional)"; opacity: 0.65 }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: prefixField
                Layout.fillWidth: true
                placeholderText: "/path/to/prefix"
            }
            RenoDialogButton {
                theme: popup.host
                text: "Browse…"
                onClicked: popup.browsePrefix(prefixField.text)
            }
        }
        Label { text: "Artwork (optional)"; opacity: 0.65 }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: artworkField
                Layout.fillWidth: true
                placeholderText: "/path/to/cover-or-banner.png"
            }
            RenoDialogButton {
                theme: popup.host
                text: "Browse…"
                onClicked: popup.browseArtwork(artworkField.text)
            }
            RenoDialogButton {
                theme: popup.host
                text: "Clear"
                visible: artworkField.text.length > 0
                onClicked: artworkField.clear()
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            opacity: 0.58
            font.pixelSize: 11
            text: "Reno119 validates the executable as a Windows PE file, detects its graphics API/architecture, and stores only this manager entry. Removing it later never deletes the program itself."
        }
        Label {
            id: errorLabel
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#e7a0a0"
            visible: text.length > 0
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
                text: popup.editing ? "Save" : "Add Program"
                onClicked: popup.commit()
            }
        }
    }
}
