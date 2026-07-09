import QtQuick
import QtQuick.Shapes

// Renders a list of polyline strokes produced by RoughPathGenerator.
Item {
    id: root
    property var strokes: []
    property color strokeColor: "#000000"
    property int strokeWidth: 1
    property real strokeOpacity: 1.0

    Repeater {
        model: root.strokes

        Shape {
            anchors.fill: parent
            opacity: root.strokeOpacity

            ShapePath {
                strokeColor: root.strokeColor
                strokeWidth: root.strokeWidth
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathPolyline {
                    path: modelData
                }
            }
        }
    }
}
