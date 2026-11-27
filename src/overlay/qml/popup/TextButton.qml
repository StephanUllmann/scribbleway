import QtQuick
import QtQuick.Controls.Basic as Controls
import ".."

Controls.Button {
    id: textButton
    property color tone: theme.ink

    Theme { id: theme }

    implicitHeight: 26
    leftPadding: 10
    rightPadding: 10
    topPadding: 0
    bottomPadding: 0
    font.family: theme.sans
    font.pixelSize: 12
    contentItem: Text {
        text: textButton.text
        font: textButton.font
        color: textButton.enabled ? textButton.tone : theme.mute
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 5
        color: textButton.down ? theme.pressed : textButton.hovered ? theme.raised : theme.surface
        border.width: textButton.visualFocus ? 1 : 0
        border.color: theme.accent
    }
}
