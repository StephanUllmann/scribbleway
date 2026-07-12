import QtQuick
import QtQuick.Shapes
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
        shadowBlur: root.modelGlow / 30.0
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Item {
        id: shapeContent
        anchors.fill: parent
        visible: root.modelGlow === 0

        Shape {
            anchors.fill: parent
            opacity: root.modelOpacity

            ShapePath {
                strokeColor: root.modelRoughness === 0 ? root.modelColor : "transparent"
                strokeWidth: root.modelStrokeWidth
                fillColor: {
                    let c = Qt.color(root.modelColor);
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

        RoughStroke {
            anchors.fill: parent
            strokes: root.modelRoughness > 0
                ? RoughPathGenerator.getSketchyEllipse(
                    root.shapeX, root.shapeY, root.shapeWidth, root.shapeHeight,
                    root.modelRoughness, root.modelSeed)
                : []
            strokeColor: root.modelColor
            strokeWidth: root.modelStrokeWidth
            strokeOpacity: root.modelOpacity
        }
    }
}
