pragma Singleton
import QtQuick

QtObject {
    readonly property int gridUnit: 18
    readonly property int smallSpacing: 4
    readonly property int largeSpacing: 8

    // Icon sizes
    readonly property int iconSmall: 16
    readonly property int iconMedium: 24
    readonly property int iconLarge: 32

    // Palette references
    property SystemPalette pal: SystemPalette {}
    readonly property color highlightColor: pal.highlight
    readonly property color disabledTextColor: Qt.alpha(pal.text, 0.5)
    readonly property color backgroundColor: pal.base
    readonly property color hoverColor: pal.midlight
    readonly property color warningColor: "#f39c12"
}
