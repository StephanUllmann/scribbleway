import QtQuick
import QtQuick.Effects
import "RoughPathGenerator.js" as RoughPathGenerator

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

    MultiEffect {
        id: glowEffect
        anchors.fill: shapeContent
        source: shapeContent
        visible: root.modelGlow > 0

        shadowEnabled: true
        shadowColor: root.modelColor
        shadowBlur: root.modelGlow / 15.0
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Item {
        id: shapeContent
        anchors.fill: parent
        visible: root.modelGlow === 0

        Rectangle {
            x: root.shapeX
            y: root.shapeY
            width: root.shapeWidth
            height: root.shapeHeight
            opacity: root.modelOpacity

            border.color: root.modelColor
            border.width: root.modelRoughness === 0 ? root.modelStrokeWidth : 0
            // Excalidraw-like premium fill style (stroke color at 12% opacity)
            color: {
                let c = Qt.color(root.modelColor);
                return Qt.rgba(c.r, c.g, c.b, 0.12);
            }
            radius: typeof borderRadius !== "undefined" ? borderRadius : 0
        }

        RoughStroke {
            anchors.fill: parent
            strokes: root.modelRoughness > 0
                ? RoughPathGenerator.getSketchyRectangle(
                    root.shapeX, root.shapeY, root.shapeWidth, root.shapeHeight,
                    root.modelRoughness, root.modelSeed,
                    typeof borderRadius !== "undefined" ? borderRadius : 0)
                : []
            strokeColor: root.modelColor
            strokeWidth: root.modelStrokeWidth
            strokeOpacity: root.modelOpacity
        }
    }
}
