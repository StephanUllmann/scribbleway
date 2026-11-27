import QtQuick
import QtQuick.Controls.Basic as Controls
import ".."

Controls.ToolButton {
    id: iconButton
    property string tip: ""

    Theme { id: theme }

    display: Controls.AbstractButton.IconOnly
    icon.width: 18
    icon.height: 18
    icon.color: enabled ? theme.ink : theme.mute
    implicitWidth: 30
    implicitHeight: 30
    padding: 0
    background: Rectangle {
        radius: theme.radius
        color: iconButton.down ? theme.pressed
             : iconButton.hovered ? theme.raised
             : iconButton.flat ? "transparent" : theme.surface
        border.width: iconButton.visualFocus ? 1 : 0
        border.color: theme.accent
    }
    Accessible.name: tip
    Controls.ToolTip.visible: hovered && tip !== ""
    Controls.ToolTip.text: tip
    Controls.ToolTip.delay: 600
}
