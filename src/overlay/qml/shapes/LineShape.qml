import QtQuick
import QtQuick.Shapes

BaseShape {
    id: root
    
    mode: "line"
    shapeIndex: index
    shapeFromX: model.fromX
    shapeFromY: model.fromY
    shapeToX: model.toX
    shapeToY: model.toY

    onLineGeometryChanged: (nfx, nfy, ntx, nty) => {
        controller.updateShape(index, { "fromX": nfx, "fromY": nfy, "toX": ntx, "toY": nty });
    }

    Shape {
        anchors.fill: parent
        opacity: model.opacity

        ShapePath {
            strokeColor: model.color
            strokeWidth: model.strokeWidth
            fillColor: "transparent"

            startX: root.shapeFromX
            startY: root.shapeFromY

            PathLine {
                x: root.shapeToX
                y: root.shapeToY
            }
        }
    }
}
