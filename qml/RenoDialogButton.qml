import QtQuick
import QtQuick.Controls

Button {
    id: buttonControl
    required property var theme
    hoverEnabled: true
    background: Item {
        implicitWidth: 96
        implicitHeight: 34
        Rectangle {
            x: -5
            y: 2
            width: parent.width + 10
            height: parent.height + 8
            radius: 10
            color: buttonControl.theme.buttonHighlight
            opacity: buttonControl.enabled && (buttonControl.hovered || buttonControl.activeFocus) ? 0.12 : 0
            Behavior on opacity { NumberAnimation { duration: 100 } }
        }
        Rectangle {
            x: -2
            y: 1
            width: parent.width + 4
            height: parent.height + 4
            radius: 8
            color: buttonControl.theme.buttonHighlight
            opacity: buttonControl.enabled && (buttonControl.hovered || buttonControl.activeFocus) ? 0.20 : 0
            Behavior on opacity { NumberAnimation { duration: 100 } }
        }
        Rectangle {
            anchors.fill: parent
            radius: 6
            color: !buttonControl.enabled ? buttonControl.theme.buttonBg
                  : buttonControl.down ? buttonControl.theme.selectedBg
                  : (buttonControl.hovered || buttonControl.activeFocus) ? buttonControl.theme.hoverBg
                  : buttonControl.theme.buttonBg
            border.width: buttonControl.enabled && (buttonControl.hovered || buttonControl.activeFocus) ? 1 : 0
            border.color: buttonControl.theme.buttonHighlight
        }
    }
    opacity: enabled ? 1.0 : 0.55
}
