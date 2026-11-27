import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls
import ".."

// A slider drag is one undo step, however many values it passes through.
Controls.Slider {
    id: slider

    Theme { id: theme }

    Layout.fillWidth: true
    onPressedChanged: pressed ? controller.beginEdit() : controller.endEdit()
    background: Rectangle {
        x: slider.leftPadding
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: slider.availableWidth
        height: 4
        radius: 2
        color: theme.raised
        Rectangle {
            width: slider.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: theme.accent
        }
    }
    handle: Rectangle {
        x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: 16
        height: 16
        radius: 8
        color: slider.pressed || slider.hovered ? "#ffffff" : theme.ink
        border.width: slider.visualFocus ? 2 : 0
        border.color: theme.accent
    }
}
