import QtQuick
import QtQuick.Shapes
import "RoughPathGenerator.js" as RoughPathGenerator
BaseShape {
    id: root

    mode: "none" // Move only, no resizing
    shapeIndex: index
    shapeX: model.bounds.x
    shapeY: model.bounds.y
    shapeWidth: Math.max(5, model.bounds.width)
    shapeHeight: Math.max(5, model.bounds.height)

    property var points: model && model.points !== undefined ? model.points : []

    onRectGeometryChanged: (nx, ny, nw, nh) => {
        let dx = nx - shapeX;
        let dy = ny - shapeY;
        let pts = root.points;
        let newPts = [];
        for (let i = 0; i < pts.length; ++i) {
            newPts.push(Qt.point(pts[i].x + dx, pts[i].y + dy));
        }
        controller.updateShape(index, { "points": newPts });
    }

        Shape {

            preferredRendererType: Shape.CurveRenderer
            anchors.fill: parent
            opacity: root.modelOpacity
            visible: root.modelRoughness === 0

            ShapePath {
                strokeColor: root.modelColor
                strokeWidth: root.modelStrokeWidth
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathPolyline {
                    path: root.points || []
                }
            }
        }

        RoughStroke {
            anchors.fill: parent
            strokes: root.modelRoughness > 0
                ? RoughPathGenerator.getSketchyFreehand(root.points, root.modelRoughness, root.modelSeed)
                : []
            strokeColor: root.modelColor
            strokeWidth: root.modelStrokeWidth
            strokeOpacity: root.modelOpacity
        }
}
