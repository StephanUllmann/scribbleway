import QtQuick
import ".."

// All popup text. The family is pinned so the popup keeps its own look whatever
// font the desktop uses for its own UI.
Text {
    Theme { id: theme }

    color: theme.ink
    font.family: theme.sans
    font.pixelSize: theme.body
}
