import QtQuick
import QtQuick.Controls.Basic as Controls
import ".."

// Opens the picker. Shows the current colour when it is not one of the presets.
Controls.ToolButton {
    id: customSwatch
    property string current: ""
    readonly property bool isCustom: current !== "" && current !== "transparent"
                                     && controller.presetColors.indexOf(current) < 0

    Theme { id: theme }

    display: Controls.AbstractButton.IconOnly
    // The current custom colour shows as the colour alone; "+" only offers a new one.
    icon.source: isCustom ? "" : "qrc:/icons/list-add.svg"
    icon.width: 14
    icon.height: 14
    icon.color: isCustom ? theme.inkOn(current) : theme.ink
    implicitWidth: 26
    implicitHeight: 26
    padding: 0
    background: Item {
        SwatchRing {
            border.color: customSwatch.isCustom ? theme.ink
                        : customSwatch.visualFocus ? theme.accent
                        : customSwatch.hovered ? theme.mute : "transparent"
        }
        Rectangle {
            anchors.fill: parent
            radius: 5
            color: customSwatch.isCustom ? customSwatch.current
                 : customSwatch.down ? theme.pressed : theme.raised
        }
    }
    Accessible.name: "Custom colour"
    Controls.ToolTip.visible: hovered
    Controls.ToolTip.text: "Custom colour"
    Controls.ToolTip.delay: 600
}
