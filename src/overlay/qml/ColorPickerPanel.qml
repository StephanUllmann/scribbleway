import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls

// Fixed-width colour picker meant to sit beside the menu inside the tray popup's
// own window. Deliberately not a QtQuick.Dialogs ColorDialog: that is an xdg-desktop-
// portal dialog, which cannot be positioned and opens fullscreen on the primary output.
Item {
    id: root

    // Two-way: assign to seed the picker, read on change. Always fully opaque —
    // the menu has its own Opacity and Fill Opacity sliders.
    property color selectedColor: "#e63946"
    signal closeRequested()

    // The host uses this to hand the keyboard over: the overlay holds an exclusive grab
    // for its hotkeys, so typing here only works while it lets go. `focus`, not
    // `activeFocus` — activeFocus additionally requires the *window* to be active, and
    // this window deliberately is not until the overlay has already let go. Waiting for
    // activeFocus would deadlock: no focus without the keyboard, no keyboard without the
    // focus.
    readonly property bool textEditing: hexField.focus

    implicitWidth: 208
    implicitHeight: layout.implicitHeight

    QtObject {
        id: theme
        readonly property int gridUnit: 18
        readonly property int smallSpacing: 4
        readonly property color highlightColor: root.palette.highlight
    }

    // HSV is the authoritative state, not selectedColor: hue survives a trip through
    // black or grey, where the RGB value on its own has forgotten it.
    property real hue: 0
    property real sat: 1
    property real val: 1

    // One latch for both directions. recompose() writes selectedColor, which re-enters
    // through onSelectedColorChanged; an external assignment goes the other way. Without
    // this the two chase each other and dragging snaps back.
    property bool updating: false

    function recompose() {
        if (updating) return
        updating = true
        selectedColor = Qt.hsva(hue, sat, val, 1)
        updating = false
    }

    function pickSaturationValue(mx, my) {
        sat = Math.max(0, Math.min(1, mx / svSquare.width))
        val = Math.max(0, Math.min(1, 1 - my / svSquare.height))
        recompose()
    }

    function pickHue(mx) {
        hue = Math.max(0, Math.min(1, mx / hueStrip.width))
        recompose()
    }

    onSelectedColorChanged: {
        // Above the latch: this fires for recompose() too, and the field froze when it
        // sat below the early return.
        if (!hexField.focus) {
            hexField.text = selectedColor.toString()
        }
        if (updating) return
        updating = true
        // hsvHue is -1 for achromatic colours (black, white, grey) — there is no hue to
        // read, so keep the one the user last chose instead of snapping the square to red.
        if (selectedColor.hsvHue >= 0) {
            hue = selectedColor.hsvHue
        }
        sat = selectedColor.hsvSaturation
        val = selectedColor.hsvValue
        updating = false
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: theme.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.smallSpacing

            Controls.Label {
                text: "Custom Color"
                font.bold: true
                font.pixelSize: 11
                color: theme.highlightColor
                Layout.fillWidth: true
            }

            Controls.ToolButton {
                text: "×"
                onClicked: root.closeRequested()
            }
        }

        // Saturation/value square: hue-tinted base, white washed out left-to-right,
        // black washed in top-to-bottom. Two gradient overlays, no Canvas, no shader.
        Rectangle {
            id: svSquare
            Layout.fillWidth: true
            Layout.preferredHeight: width
            radius: 4
            color: Qt.hsva(root.hue, 1, 1, 1)

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#ffffffff" }
                    GradientStop { position: 1.0; color: "#00ffffff" }
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#00000000" }
                    GradientStop { position: 1.0; color: "#ff000000" }
                }
            }

            // Double ring: white reads on the dark corner, black on the light one.
            Rectangle {
                width: 12
                height: 12
                radius: width / 2
                color: "transparent"
                border.width: 2
                border.color: "white"
                x: root.sat * svSquare.width - width / 2
                y: (1 - root.val) * svSquare.height - height / 2

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: width / 2
                    color: "transparent"
                    border.width: 1
                    border.color: "#60000000"
                }
            }

            MouseArea {
                anchors.fill: parent
                onPressed: (mouse) => root.pickSaturationValue(mouse.x, mouse.y)
                onPositionChanged: (mouse) => {
                    if (pressed) root.pickSaturationValue(mouse.x, mouse.y)
                }
            }
        }

        // Drawn, not a Controls.Slider. Under org.kde.desktop the whole slider — groove
        // *and* handle — is painted by QStyle into the background item, so replacing the
        // background to get this gradient left it with nothing to grab. Drawing it the
        // same way as the square above also keeps the picker identical across styles.
        Rectangle {
            id: hueStrip
            objectName: "hueStrip"
            Layout.fillWidth: true
            Layout.topMargin: theme.smallSpacing
            Layout.preferredHeight: 16
            radius: 4
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.000; color: "#ff0000" }
                GradientStop { position: 0.167; color: "#ffff00" }
                GradientStop { position: 0.333; color: "#00ff00" }
                GradientStop { position: 0.500; color: "#00ffff" }
                GradientStop { position: 0.667; color: "#0000ff" }
                GradientStop { position: 0.833; color: "#ff00ff" }
                GradientStop { position: 1.000; color: "#ff0000" }
            }

            Rectangle {
                width: 7
                height: parent.height + 6
                y: -3
                x: root.hue * hueStrip.width - width / 2
                radius: 3
                color: "transparent"
                border.width: 2
                border.color: "white"

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -1
                    radius: 4
                    color: "transparent"
                    border.width: 1
                    border.color: "#60000000"
                }
            }

            MouseArea {
                anchors.fill: parent
                onPressed: (mouse) => root.pickHue(mouse.x)
                onPositionChanged: (mouse) => {
                    if (pressed) root.pickHue(mouse.x)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.smallSpacing

            Rectangle {
                Layout.preferredWidth: theme.gridUnit * 1.5
                Layout.preferredHeight: theme.gridUnit * 1.5
                radius: 4
                color: root.selectedColor
                border.width: 1
                border.color: "gray"
            }

            Controls.TextField {
                id: hexField
                objectName: "hexField"
                Layout.fillWidth: true
                text: root.selectedColor.toString()
                validator: RegularExpressionValidator {
                    regularExpression: /^#?[0-9A-Fa-f]{6}$/
                }
                onAccepted: {
                    root.selectedColor = text.startsWith("#") ? text : "#" + text
                }
            }
        }
    }
}
