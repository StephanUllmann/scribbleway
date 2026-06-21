import QtQuick
import QtQuick.Shapes

BaseShape {
    id: root
    
    mode: "none" // Move only, no resizing
    shapeIndex: index
    isSelected: model.selected
    isLocked: model.locked

    // Compute bounding box from point list for selection and drag bounds
    property real calculatedMinX: 0
    property real calculatedMinY: 0
    property real calculatedWidth: 0
    property real calculatedHeight: 0

    shapeX: calculatedMinX
    shapeY: calculatedMinY
    shapeWidth: calculatedWidth
    shapeHeight: calculatedHeight

    property var points: {
        let pts = model.points;
        if (!pts || typeof pts !== "object" || !pts.length) return [];
        let mapped = [];
        for (let i = 0; i < pts.length; ++i) {
            let p = pts[i];
            if (p) {
                mapped.push(Qt.point(p.x, p.y));
            }
        }
        return mapped;
    }
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
        opacity: model.opacity
        
        ShapePath {
            strokeColor: model.color
            strokeWidth: model.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathPolyline {
                path: root.points || []
            }
        }
    }
}
