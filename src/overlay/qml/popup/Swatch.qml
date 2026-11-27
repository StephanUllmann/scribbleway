import QtQuick
import QtQuick.Controls.Basic as Controls
import ".."

Controls.AbstractButton {
    id: swatch
    property color swatchColor: "transparent"
    property bool none: false     // the "no fill" cell
    property bool selected: false
    property string hotkey: ""

    Theme { id: theme }

    implicitWidth: 26
    implicitHeight: 26
    leftPadding: 3
    bottomPadding: 2
    background: Item {
        SwatchRing {
            border.color: swatch.selected ? theme.ink
                        : swatch.visualFocus ? theme.accent
                        : swatch.hovered ? theme.mute : "transparent"
        }
        Rectangle {
            anchors.fill: parent
            radius: 5
            clip: true
            color: swatch.none ? theme.surface : swatch.swatchColor
            border.width: swatch.none ? 1 : 0
            border.color: theme.raised
            Rectangle {
                visible: swatch.none
                anchors.centerIn: parent
                width: parent.width * 1.2
                height: 2
                rotation: -45
                color: theme.danger
            }
        }
    }
    contentItem: Text {
        text: swatch.hotkey
        font.family: theme.mono
        font.pixelSize: theme.small
        font.weight: Font.Bold
        color: theme.inkOn(swatch.swatchColor)
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignBottom
    }
}
