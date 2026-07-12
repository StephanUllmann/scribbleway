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

    // Geometry math for arrowhead
    readonly property real lineAngle: Math.atan2(shapeToY - shapeFromY, shapeToX - shapeFromX)
    readonly property real arrowLength: 10 + root.modelStrokeWidth * 1.5
    readonly property real arrowHalfAngle: Math.PI / 6 // 30 degrees

    readonly property real arrowLeftX: shapeToX - arrowLength * Math.cos(lineAngle - arrowHalfAngle)
    readonly property real arrowLeftY: shapeToY - arrowLength * Math.sin(lineAngle - arrowHalfAngle)

    readonly property real arrowRightX: shapeToX - arrowLength * Math.cos(lineAngle + arrowHalfAngle)
    readonly property real arrowRightY: shapeToY - arrowLength * Math.sin(lineAngle + arrowHalfAngle)

    // Calculate midpoint at the bottom/base of the arrowhead
    readonly property real lineLength: Math.sqrt(Math.pow(shapeToX - shapeFromX, 2) + Math.pow(shapeToY - shapeFromY, 2))
    readonly property real stemLength: Math.max(0, lineLength - arrowLength * Math.cos(arrowHalfAngle))
    readonly property real arrowBaseX: shapeFromX + stemLength * Math.cos(lineAngle)
    readonly property real arrowBaseY: shapeFromY + stemLength * Math.sin(lineAngle)

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
            preferredRendererType: Shape.CurveRenderer
            visible: root.modelRoughness === 0

            // Arrow Stem (Line)
            ShapePath {
                strokeColor: root.modelColor
                strokeWidth: root.modelStrokeWidth
                fillColor: "transparent"

                startX: root.shapeFromX
                startY: root.shapeFromY

                PathLine {
                    x: root.arrowBaseX
                    y: root.arrowBaseY
                }
            }

            // Arrowhead (Solid Triangle)
            ShapePath {
                strokeColor: "transparent"
                fillColor: root.modelColor

                startX: root.shapeToX
                startY: root.shapeToY

                PathLine {
                    x: root.arrowLeftX
                    y: root.arrowLeftY
                }

                PathLine {
                    x: root.arrowRightX
                    y: root.arrowRightY
                }

                PathLine {
                    x: root.shapeToX
                    y: root.shapeToY
                }
            }
        }

        RoughStroke {
            anchors.fill: parent
            strokes: root.modelRoughness > 0
                ? RoughPathGenerator.getSketchyArrow(
                    root.shapeFromX, root.shapeFromY, root.shapeToX, root.shapeToY,
                    root.modelRoughness, root.modelSeed, root.arrowLength)
                : []
            strokeColor: root.modelColor
            strokeWidth: root.modelStrokeWidth
            strokeOpacity: root.modelOpacity
        }
    }
}
