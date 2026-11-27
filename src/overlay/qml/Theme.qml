import QtQuick

// The Tray Popup's own dark look, the same on every platform (see PRODUCT.md). It never
// reads the system palette: a palette role that collapses under some theme was what made
// the old popup unreadable. Every text colour here clears WCAG AA on `surface` and `ground`.
QtObject {
    readonly property color ground: "#111111"
    readonly property color well: "#0a0a0a"      // the specimen well, a step below the ground
    readonly property color surface: "#1c1c1c"   // buttons, segmented tracks, swatch frames
    readonly property color raised: "#2c2c2c"    // checked segment, hover, slider groove
    readonly property color pressed: "#363636"
    readonly property color line: "#2a2a2a"
    readonly property color ink: "#f1efe9"
    readonly property color mute: "#a3a09a"      // labels and secondary text, 6.5:1 on surface
    readonly property color accent: "#a99bff"    // current tool, slider fill, focus
    readonly property color accentInk: "#111111"
    readonly property color selectedRow: "#272335" // accent-tinted, for the selected list row
    readonly property color danger: "#ff8a80"

    readonly property int body: 13
    readonly property int small: 11
    readonly property int pad: 12
    readonly property int gap: 8
    readonly property int radius: 7
    readonly property int labelWidth: 78
    readonly property int valueWidth: 40
    // Generic families: fontconfig/CoreText resolve them to the platform's sans and mono
    // faces (Noto Sans and DejaVu Sans Mono on most Linux desktops), not to the desktop's
    // chosen UI font, and no font files need to ship.
    readonly property string sans: "sans-serif"
    // CoreText has no reliable "monospace" alias, so macOS names its stock mono face.
    readonly property string mono: Qt.platform.os === "osx" ? "Menlo" : "monospace"

    // Near-black or near-white, whichever reads better on `c` (WCAG relative luminance).
    function inkOn(c) {
        const lin = v => v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4)
        const col = Qt.color(c)
        const l = 0.2126 * lin(col.r) + 0.7152 * lin(col.g) + 0.0722 * lin(col.b)
        // Contrast against #111 (L≈0.0056) vs against #fff (L=1); pick the larger.
        return (l + 0.05) / 0.0556 >= 1.05 / (l + 0.05) ? "#111111" : "#ffffff"
    }
}
