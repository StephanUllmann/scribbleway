import QtQuick
import QtTest
import "../../src/overlay/qml"

// The picker keeps HSV, not RGB, as its authoritative state, and feeds selectedColor
// in both directions through one latch. Both of those are easy to get subtly wrong and
// invisible until someone drags through grey, so they are pinned here.
TestCase {
    id: testCase
    name: "ColorPickerPanel"
    when: windowShown
    width: 208
    height: 400
    visible: true

    ColorPickerPanel {
        id: picker
        anchors.fill: parent
    }

    function init() {
        picker.selectedColor = "#e63946"
    }

    function test_seed_decomposes_to_hsv() {
        fuzzyCompare(picker.hue, 0.9944, 0.01)
        fuzzyCompare(picker.sat, 0.7565, 0.01)
        fuzzyCompare(picker.val, 0.9020, 0.01)
    }

    function test_round_trip_is_lossless() {
        picker.recompose()
        compare(picker.selectedColor.toString(), "#e63946")
    }

    function test_drag_recomposes_without_disturbing_hue() {
        var hueBefore = picker.hue
        picker.pickSaturationValue(0, 0)
        compare(picker.selectedColor.toString(), "#ffffff")
        compare(picker.hue, hueBefore)
    }

    function test_drag_clamps_outside_the_square() {
        picker.pickSaturationValue(-50, -50)
        compare(picker.sat, 0)
        compare(picker.val, 1)
        picker.pickSaturationValue(99999, 99999)
        compare(picker.sat, 1)
        compare(picker.val, 0)
    }

    // hsvHue is -1 for achromatic colours. Reading it blindly snaps the square to red
    // the moment the user drags into the grey column.
    function test_achromatic_keeps_the_last_hue() {
        var hueBefore = picker.hue
        picker.selectedColor = "#808080"
        compare(picker.hue, hueBefore)
        fuzzyCompare(picker.sat, 0, 0.01)

        picker.sat = 1
        picker.val = 1
        picker.recompose()
        fuzzyCompare(picker.selectedColor.hsvHue, hueBefore, 0.01)
    }

    function test_assign_then_recompose_does_not_drift() {
        picker.selectedColor = "#2a9d8f"
        picker.recompose()
        compare(picker.selectedColor.toString(), "#2a9d8f")
    }
}
