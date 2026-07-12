import QtQuick
import QtQuick.Shapes
import "RoughPathGenerator.js" as RoughPathGenerator
BaseShape {
    id: root

    mode: "none" // Move only, no resizing
    shapeIndex: index
    // Compute bounding box from point list for selection and drag bounds
    property real calculatedMinX: 0
    property real calculatedMinY: 0
    property real calculatedWidth: 0
    property real calculatedHeight: 0

    shapeX: calculatedMinX
    shapeY: calculatedMinY
    shapeWidth: calculatedWidth
    shapeHeight: calculatedHeight

    property var points: model && model.points !== undefined ? model.points : []
    onPointsChanged: recalculateBounds()

    onRectGeometryChanged: (nx, ny, nw, nh) => {
        let dx = nx - calculatedMinX;
        let dy = ny - calculatedMinY;
        let pts = root.points;
        let newPts = [];
        for (let i = 0; i < pts.length; ++i) {
            newPts.push(Qt.point(pts[i].x + dx, pts[i].y + dy));
        }
        controller.updateShape(index, { "points": newPts });
    }

    Component.onCompleted: recalculateBounds()

    function recalculateBounds() {
        let pts = root.points;
        if (!pts || pts.length === 0) return;
        let minX = pts[0].x;
        let maxX = pts[0].x;
        let minY = pts[0].y;
        let maxY = pts[0].y;
        for (let i = 1; i < pts.length; ++i) {
            let p = pts[i];
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
        }
        calculatedMinX = minX;
        calculatedMinY = minY;
        calculatedWidth = Math.max(5, maxX - minX);
        calculatedHeight = Math.max(5, maxY - minY);
    }

        Shape {
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
