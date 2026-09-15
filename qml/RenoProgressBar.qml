import QtQuick

Rectangle {
    id: root

    property real value: 0.0
    property bool indeterminate: false
    property color trackColor: "#343841"
    property color fillColor: "#8257e5"

    implicitHeight: 6
    radius: height / 2
    color: trackColor
    clip: true

    Rectangle {
        id: indicator

        property real phase: 0.0

        height: parent.height
        width: root.indeterminate
               ? Math.max(24, root.width * 0.28)
               : root.width * Math.max(0.0, Math.min(1.0, root.value))
        radius: root.radius
        color: root.fillColor
        x: root.indeterminate
           ? -width + (root.width + width) * phase
           : 0

        NumberAnimation on phase {
            from: 0.0
            to: 1.0
            duration: 900
            loops: Animation.Infinite
            running: root.indeterminate && root.visible && root.width > 0
        }
    }
}
