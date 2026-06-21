import QtQuick
import QtQuick.Shapes

BaseShape {
    id: root
    
    mode: "rect"
    shapeIndex: index
    isSelected: model.selected
    isLocked: model.locked

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
            // Excalidraw-like premium fill style (stroke color at 12% opacity)
            fillColor: {
                let c = Qt.color(model.color);
                return Qt.rgba(c.r, c.g, c.b, 0.12);
            }
            
            startX: root.shapeX
            startY: root.shapeY
            
            PathLine { x: root.shapeX + root.shapeWidth; y: root.shapeY }
            PathLine { x: root.shapeX + root.shapeWidth; y: root.shapeY + root.shapeHeight }
            PathLine { x: root.shapeX; y: root.shapeY + root.shapeHeight }
            PathLine { x: root.shapeX; y: root.shapeY }
        }
    }
}
