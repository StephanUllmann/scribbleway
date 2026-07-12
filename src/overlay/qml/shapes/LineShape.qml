import QtQuick
import QtQuick.Shapes
import QtQuick.Effects
import "RoughPathGenerator.js" as RoughPathGenerator

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

        Shape {
            anchors.fill: parent
            opacity: root.modelOpacity
            visible: root.modelRoughness === 0

            ShapePath {
                strokeColor: root.modelColor
                strokeWidth: root.modelStrokeWidth
                fillColor: "transparent"

                startX: root.shapeFromX
                startY: root.shapeFromY

                PathLine {
                    x: root.shapeToX
                    y: root.shapeToY
                }
            }
        }

        RoughStroke {
            anchors.fill: parent
            strokes: root.modelRoughness > 0
                ? RoughPathGenerator.getSketchyLine(
                    root.shapeFromX, root.shapeFromY, root.shapeToX, root.shapeToY,
                    root.modelRoughness, root.modelSeed)
                : []
            strokeColor: root.modelColor
            strokeWidth: root.modelStrokeWidth
            strokeOpacity: root.modelOpacity
        }
    }
}
