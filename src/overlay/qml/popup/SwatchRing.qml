import QtQuick

// The ring just outside a swatch that marks it as picked, focused or hovered.
// The owner sets `border.color`; transparent hides the ring.
Rectangle {
    anchors.fill: parent
    anchors.margins: -3
    radius: 8
    color: "transparent"
    border.width: 2
}
