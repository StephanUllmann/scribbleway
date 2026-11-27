import QtQuick
import QtTest
import "../../src/overlay/qml"

// The hotkey digit on each preset swatch takes its colour from inkOn(). A wrong pick
// makes the digit vanish into the swatch, so the light and dark presets are pinned here.
TestCase {
    name: "Theme"

    Theme {
        id: theme
    }

    function test_ink_on_presets() {
        compare(theme.inkOn("#e9c46a"), "#111111")   // yellow: dark digit
        compare(theme.inkOn("#f4a261"), "#111111")   // orange: dark digit
        compare(theme.inkOn("#8338ec"), "#ffffff")   // violet: light digit
        compare(theme.inkOn("#000000"), "#ffffff")
        compare(theme.inkOn("#ffffff"), "#111111")
    }
}
