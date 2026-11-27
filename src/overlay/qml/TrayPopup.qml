import QtQuick
import QtQuick.Window
import QtQuick.Layouts

Window {
    id: popupWindow

    readonly property int menuWidth: 432

    // The surface is layer-shell anchored Bottom|Right (see traypopup.h), so its right edge
    // is pinned and the extra width for the picker grows out to the left. The menu keeps
    // its own width either way.
    width: menuWidth + (picker.visible ? picker.implicitWidth + 2 * theme.pad + 1 : 0)
    height: 550
    minimumWidth: 400
    minimumHeight: 520
    flags: Qt.Popup | Qt.FramelessWindowHint
    visible: false
    color: theme.ground

    property bool pickerIsFill: false
    onVisibleChanged: if (!visible) picker.visible = false

    Theme {
        id: theme
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // The Basic controls inside read these, so the parts no component restyles
        // (tooltips, the combo box popup, text fields) still match the popup's own look.
        palette.window: theme.ground
        palette.windowText: theme.ink
        palette.base: theme.surface
        palette.alternateBase: theme.raised
        palette.text: theme.ink
        palette.button: theme.surface
        palette.buttonText: theme.ink
        palette.brightText: theme.ink
        palette.highlight: theme.accent
        palette.highlightedText: theme.accentInk
        palette.light: theme.raised
        palette.midlight: theme.raised
        palette.mid: theme.pressed
        palette.dark: theme.pressed
        palette.shadow: "#000000"
        palette.toolTipBase: theme.raised
        palette.toolTipText: theme.ink
        palette.placeholderText: theme.mute

        ColorPickerPanel {
            id: picker
            visible: false
            Layout.preferredWidth: implicitWidth
            Layout.alignment: Qt.AlignTop
            Layout.margins: theme.pad

            // Live, like every slider in the menu — the popup no longer dismisses on
            // interaction, so there is nothing to confirm. Only while visible: seeding
            // the panel on open must not write the colour straight back, or opening the
            // fill picker over a transparent fill would silently make it opaque.
            onSelectedColorChanged: {
                if (visible) menu.applyCustomColor(selectedColor, popupWindow.pickerIsFill)
            }
            onCloseRequested: visible = false
            onDraggingChanged: dragging ? controller.beginEdit() : controller.endEdit()

            // Hand the keyboard back and forth as the hex field takes and loses focus,
            // so the overlay keeps its hotkeys the rest of the time.
            onTextEditingChanged: trayPopup.wantsKeyboard = textEditing
            onVisibleChanged: if (!visible) trayPopup.wantsKeyboard = false
        }

        Rectangle {
            visible: picker.visible
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: theme.line
        }

        FullRepresentation {
            id: menu
            Layout.fillWidth: true
            Layout.fillHeight: true

            onCustomColorRequested: (current, isFill) => {
                picker.visible = false   // suppress the apply while seeding — see above
                popupWindow.pickerIsFill = isFill
                picker.selectedColor = current
                picker.visible = true
            }
        }
    }

    // A hairline edge, so the dark popup reads as a surface against a dark desktop.
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: theme.line
    }
}
