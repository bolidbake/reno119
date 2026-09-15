import QtQuick
import QtQuick.Controls

ComboBox {
    id: comboControl
    wheelEnabled: false

    // Consume the event before a style/content item can handle it. Ignoring
    // it here would let the same wheel event reach the ComboBox again.
    // Mouse clicks pass through; the open popup keeps its normal scrolling.
    MouseArea {
        anchors.fill: parent
        z: 1000
        enabled: !comboControl.popup || !comboControl.popup.visible
        acceptedButtons: Qt.NoButton
        scrollGestureEnabled: true
        onWheel: function(wheel) {
            wheel.accepted = true
            let dx = wheel.pixelDelta.x !== 0 ? wheel.pixelDelta.x : wheel.angleDelta.x / 120 * 60
            let dy = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y : wheel.angleDelta.y / 120 * 60
            // Deltas already follow the system's natural-scroll setting.
            let ancestor = comboControl.parent
            while (ancestor && (dx !== 0 || dy !== 0)) {
                if (ancestor.contentY !== undefined && ancestor.contentHeight !== undefined
                        && ancestor.originY !== undefined) {
                    if (dy !== 0) {
                        const minimum = ancestor.originY - (ancestor.topMargin || 0)
                        const maximum = Math.max(minimum, ancestor.originY + ancestor.contentHeight
                                                 + (ancestor.bottomMargin || 0) - ancestor.height)
                        const next = Math.max(minimum, Math.min(maximum, ancestor.contentY - dy))
                        if (next !== ancestor.contentY) {
                            ancestor.cancelFlick()
                            ancestor.contentY = next
                            dy = 0
                        }
                    }
                    if (dx !== 0 && ancestor.originX !== undefined) {
                        const minimum = ancestor.originX - (ancestor.leftMargin || 0)
                        const maximum = Math.max(minimum, ancestor.originX + ancestor.contentWidth
                                                 + (ancestor.rightMargin || 0) - ancestor.width)
                        const next = Math.max(minimum, Math.min(maximum, ancestor.contentX - dx))
                        if (next !== ancestor.contentX) {
                            ancestor.cancelFlick()
                            ancestor.contentX = next
                            dx = 0
                        }
                    }
                }
                ancestor = ancestor.parent
            }
        }
    }
}
