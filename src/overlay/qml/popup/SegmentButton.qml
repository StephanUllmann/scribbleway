import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls
import ".."

Controls.AbstractButton {
    id: segment

    Theme { id: theme }

    implicitHeight: 26
    implicitWidth: segmentLabel.implicitWidth + 20
    Layout.fillWidth: true
    contentItem: Label {
        id: segmentLabel
        text: segment.text
        color: segment.checked ? theme.ink : theme.mute
        font.weight: segment.checked ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: theme.radius - 2
        color: segment.checked ? theme.raised : segment.hovered ? "#242424" : "transparent"
        border.width: segment.visualFocus ? 1 : 0
        border.color: theme.accent
    }
}
