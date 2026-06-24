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

    Rectangle {
        x: root.shapeX
        y: root.shapeY
        width: root.shapeWidth
        height: root.shapeHeight
        opacity: model.opacity
        
        border.color: model.color
        border.width: model.strokeWidth
        // Excalidraw-like premium fill style (stroke color at 12% opacity)
        color: {
            let c = Qt.color(model.color);
            return Qt.rgba(c.r, c.g, c.b, 0.12);
        }
        radius: model.borderRadius !== undefined ? model.borderRadius : 0
    }
}
