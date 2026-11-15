import QtQuick

// The dark size pill ("120 px", "200 × 100 px") shown while drawing or resizing a shape.
Rectangle {
    property alias text: label.text

    width: label.implicitWidth + 10
    height: label.implicitHeight + 4
    radius: 4
    color: "#cc000000"
    z: 999

    Text {
        id: label
        anchors.centerIn: parent
        color: "white"
        font.pixelSize: 12
    }
}
