import QtQuick
import QtQuick.Shapes

BaseShape {
    id: root
    
    mode: "rect"
    shapeIndex: index
    shapeX: model.x
    shapeY: model.y
    shapeWidth: model.width
    shapeHeight: model.height

    onRectGeometryChanged: (nx, ny, nw, nh) => {
        controller.updateShape(index, { "x": nx, "y": ny, "width": nw, "height": nh });
    }

    Shape {
        anchors.fill: parent
        opacity: model.opacity

        ShapePath {
            strokeColor: model.color
            strokeWidth: model.strokeWidth
            fillColor: {
                let c = Qt.color(model.color);
                return Qt.rgba(c.r, c.g, c.b, 0.12);
            }

            // Start at top center
            startX: root.shapeX + root.shapeWidth / 2
            startY: root.shapeY

            // Draw to bottom center
            PathArc {
                x: root.shapeX + root.shapeWidth / 2
                y: root.shapeY + root.shapeHeight
                radiusX: root.shapeWidth / 2
                radiusY: root.shapeHeight / 2
                useLargeArc: false
                direction: PathArc.Clockwise
            }

            // Draw back to top center
            PathArc {
                x: root.shapeX + root.shapeWidth / 2
                y: root.shapeY
                radiusX: root.shapeWidth / 2
                radiusY: root.shapeHeight / 2
                useLargeArc: false
                direction: PathArc.Clockwise
            }
        }
    }
}
