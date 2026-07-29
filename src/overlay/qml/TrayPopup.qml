import QtQuick
import QtQuick.Window
import QtQuick.Layouts

Window {
    id: trayPopup

    readonly property int menuWidth: 432
    readonly property int gap: 8

    // The surface is layer-shell anchored Bottom|Right (see main.cpp), so its right edge
    // is pinned and the extra width for the picker grows out to the left. The menu keeps
    // its own width either way.
    width: menuWidth + (picker.visible ? picker.implicitWidth + gap : 0)
    height: 550
    minimumWidth: 400
    minimumHeight: 520
    flags: Qt.Popup | Qt.FramelessWindowHint
    visible: false
    color: sysPalette.window

    property bool pickerIsFill: false

    SystemPalette {
        id: sysPalette
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: trayPopup.gap

        ColorPickerPanel {
            id: picker
            visible: false
            Layout.preferredWidth: implicitWidth
            Layout.alignment: Qt.AlignTop

            // Live, like every slider in the menu — the popup no longer dismisses on
            // interaction, so there is nothing to confirm. Only while visible: seeding
            // the panel on open must not write the colour straight back, or opening the
            // fill picker over a transparent fill would silently make it opaque.
            onSelectedColorChanged: {
                if (visible) menu.applyCustomColor(selectedColor, trayPopup.pickerIsFill)
            }
            onCloseRequested: visible = false
        }

        FullRepresentation {
            id: menu
            Layout.fillWidth: true
            Layout.fillHeight: true

            onCustomColorRequested: (current, isFill) => {
                picker.visible = false   // suppress the apply while seeding — see above
                trayPopup.pickerIsFill = isFill
                picker.selectedColor = current
                picker.visible = true
            }
        }
    }
}
