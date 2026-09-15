import QtQuick

Rectangle {
    id: control

    property alias text: editor.text
    property alias readOnly: editor.readOnly
    property alias selectByMouse: editor.selectByMouse
    property alias textFormat: editor.textFormat
    property alias wrapMode: editor.wrapMode
    property alias font: editor.font
    property string placeholderText: ""
    property color backgroundColor: "transparent"
    property color foregroundColor: "white"
    property color borderColor: "transparent"
    property color placeholderColor: "#808080"
    property color selectionColor: "#405080"
    property color selectedTextColor: foregroundColor
    property int contentPadding: 8

    implicitWidth: 240
    implicitHeight: Math.max(40, editor.contentHeight + contentPadding * 2)
    color: backgroundColor
    border.color: borderColor
    border.width: borderColor.a > 0 ? 1 : 0
    radius: 4
    clip: true

    TextEdit {
        id: editor
        anchors.fill: parent
        anchors.margins: control.contentPadding
        color: control.foregroundColor
        selectionColor: control.selectionColor
        selectedTextColor: control.selectedTextColor
        verticalAlignment: TextEdit.AlignTop
        focus: false
    }

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: control.contentPadding
        visible: !control.readOnly && editor.text.length === 0 && control.placeholderText.length > 0
        text: control.placeholderText
        color: control.placeholderColor
        wrapMode: Text.Wrap
        font: editor.font
        opacity: 0.75
    }
}
