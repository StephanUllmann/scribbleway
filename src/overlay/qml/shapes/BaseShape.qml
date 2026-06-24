import QtQuick

Item {
    id: baseShapeRoot
    anchors.fill: parent

    property int index: -1
    property int shapeIndex: index
    property string mode: "rect" // "rect", "line", "none" (freehand)
    property bool isSelected: model.selected
    property bool isLocked: model.locked

    // Rect-mode properties
    property real shapeX: 0
    property real shapeY: 0
    property real shapeWidth: 0
    property real shapeHeight: 0
    property real minSize: 15

    property bool isResizing: false
    property bool isInteracting: dragArea.pressed || isResizing

    // Line-mode properties
    property real shapeFromX: 0
    property real shapeFromY: 0
    property real shapeToX: 0
    property real shapeToY: 0

    // Signals to update coordinates back to model
    signal rectGeometryChanged(real x, real y, real w, real h)
    signal lineGeometryChanged(real fx, real fy, real tx, real ty)
    signal doubleClicked(var mouse)

    // Main interaction MouseArea (Moves shape)
    MouseArea {
        id: dragArea
        x: mode === "line" ? Math.min(shapeFromX, shapeToX) - 10 : shapeX
        y: mode === "line" ? Math.min(shapeFromY, shapeToY) - 10 : shapeY
        width: mode === "line" ? Math.abs(shapeFromX - shapeToX) + 20 : shapeWidth
        height: mode === "line" ? Math.abs(shapeFromY - shapeToY) + 20 : shapeHeight
        enabled: !isLocked
        
        property real startMouseX: 0
        property real startMouseY: 0
        property real startShapeX: 0
        property real startShapeY: 0
        
        // Endpoint start drag
        property real startFX: 0
        property real startFY: 0
        property real startTX: 0
        property real startTY: 0

        onPressed: (mouse) => {
            controller.selectShape(shapeIndex, mouse.modifiers & Qt.ShiftModifier);
            controller.beginEdit();
            let pos = mapToItem(baseShapeRoot, mouse.x, mouse.y);
            startMouseX = pos.x;
            startMouseY = pos.y;
            startShapeX = shapeX;
            startShapeY = shapeY;
            startFX = shapeFromX;
            startFY = shapeFromY;
            startTX = shapeToX;
            startTY = shapeToY;
        }

        onReleased: {
            controller.endEdit();
        }

        onPositionChanged: (mouse) => {
            if (pressed) {
                let pos = mapToItem(baseShapeRoot, mouse.x, mouse.y);
                let dx = pos.x - startMouseX;
                let dy = pos.y - startMouseY;
                if (isSelected) {
                    controller.dragSelected(dx, dy);
                } else {
                    if (mode === "line") {
                        lineGeometryChanged(startFX + dx, startFY + dy, startTX + dx, startTY + dy);
                    } else {
                        rectGeometryChanged(startShapeX + dx, startShapeY + dy, shapeWidth, shapeHeight);
                    }
                }
            }
        }

        onDoubleClicked: (mouse) => {
            baseShapeRoot.doubleClicked(mouse);
        }
    }

    // Selection border & handles UI
    Item {
        visible: isSelected && !isLocked

        // Thin blue border
        Rectangle {
            visible: mode !== "line"
            x: mode === "line" ? Math.min(shapeFromX, shapeToX) : shapeX
            y: mode === "line" ? Math.min(shapeFromY, shapeToY) : shapeY
            width: mode === "line" ? Math.abs(shapeFromX - shapeToX) : shapeWidth
            height: mode === "line" ? Math.abs(shapeFromY - shapeToY) : shapeHeight
            color: "transparent"
            border.color: "#3b82f6"
            border.width: 1
        }

        // Corner & Midpoint Handles (Rect Mode)
        Repeater {
            model: (mode === "rect" && !controller.hasMultiSelection) ? 8 : 0
            Rectangle {
                width: 10
                height: 10
                color: "#2563eb"
                border.color: "white"
                border.width: 1
                radius: 2
                
                // Position handles based on index
                x: {
                    let hs = width / 2;
                    switch(index) {
                        case 0: return shapeX - hs; // Top-Left
                        case 1: return shapeX + shapeWidth/2 - hs; // Top-Middle
                        case 2: return shapeX + shapeWidth - hs; // Top-Right
                        case 3: return shapeX + shapeWidth - hs; // Middle-Right
                        case 4: return shapeX + shapeWidth - hs; // Bottom-Right
                        case 5: return shapeX + shapeWidth/2 - hs; // Bottom-Middle
                        case 6: return shapeX - hs; // Bottom-Left
                        case 7: return shapeX - hs; // Middle-Left
                    }
                    return 0;
                }
                
                y: {
                    let hs = height / 2;
                    switch(index) {
                        case 0: return shapeY - hs;
                        case 1: return shapeY - hs;
                        case 2: return shapeY - hs;
                        case 3: return shapeY + shapeHeight/2 - hs;
                        case 4: return shapeY + shapeHeight - hs;
                        case 5: return shapeY + shapeHeight - hs;
                        case 6: return shapeY + shapeHeight - hs;
                        case 7: return shapeY + shapeHeight/2 - hs;
                    }
                    return 0;
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: {
                        switch(index) {
                            case 0:
                            case 4: return Qt.SizeFDiagCursor;
                            case 2:
                            case 6: return Qt.SizeBDiagCursor;
                            case 1:
                            case 5: return Qt.SizeVerCursor;
                            case 3:
                            case 7: return Qt.SizeHorCursor;
                        }
                        return Qt.ArrowCursor;
                    }

                    property real dragStartX: 0
                    property real dragStartY: 0
                    property real originalX: 0
                    property real originalY: 0
                    property real originalW: 0
                    property real originalH: 0

                    onPressed: (mouse) => {
                        controller.beginEdit();
                        baseShapeRoot.isResizing = true;
                        let pos = mapToItem(baseShapeRoot, mouse.x, mouse.y);
                        dragStartX = pos.x;
                        dragStartY = pos.y;
                        originalX = shapeX;
                        originalY = shapeY;
                        originalW = shapeWidth;
                        originalH = shapeHeight;
                    }

                    onReleased: {
                        baseShapeRoot.isResizing = false;
                        controller.endEdit();
                    }

                    onPositionChanged: (mouse) => {
                        if (pressed) {
                            let pos = mapToItem(baseShapeRoot, mouse.x, mouse.y);
                            let dx = pos.x - dragStartX;
                            let dy = pos.y - dragStartY;
                            let nx = originalX;
                            let ny = originalY;
                            let nw = originalW;
                            let nh = originalH;

                            switch(index) {
                                case 0: // Top-Left
                                    nx += dx; ny += dy; nw -= dx; nh -= dy; break;
                                case 1: // Top-Middle
                                    ny += dy; nh -= dy; break;
                                case 2: // Top-Right
                                    ny += dy; nw += dx; nh -= dy; break;
                                case 3: // Middle-Right
                                    nw += dx; break;
                                case 4: // Bottom-Right
                                    nw += dx; nh += dy; break;
                                case 5: // Bottom-Middle
                                    nh += dy; break;
                                case 6: // Bottom-Left
                                    nx += dx; nw -= dx; nh += dy; break;
                                case 7: // Middle-Left
                                    nx += dx; nw -= dx; break;
                            }

                            if (nw < minSize) {
                                nx = originalX + originalW - minSize;
                                nw = minSize;
                            }
                            if (nh < minSize) {
                                ny = originalY + originalH - minSize;
                                nh = minSize;
                            }

                            rectGeometryChanged(nx, ny, nw, nh);
                        }
                    }
                }
            }
        }

        // Endpoint Handles (Line Mode)
        Repeater {
            model: mode === "line" ? 2 : 0
            Rectangle {
                width: 12
                height: 12
                color: "#1d4ed8"
                border.color: "white"
                border.width: 1.5
                radius: 6
                
                x: (index === 0 ? shapeFromX : shapeToX) - width/2
                y: (index === 0 ? shapeFromY : shapeToY) - height/2

                MouseArea {
                    anchors.fill: parent
                    enabled: !controller.hasMultiSelection
                    cursorShape: enabled ? Qt.SizeAllCursor : Qt.ArrowCursor

                    property real dragStartX: 0
                    property real dragStartY: 0
                    property real originalX: 0
                    property real originalY: 0

                    onPressed: (mouse) => {
                        controller.beginEdit();
                        baseShapeRoot.isResizing = true;
                        let pos = mapToItem(baseShapeRoot, mouse.x, mouse.y);
                        dragStartX = pos.x;
                        dragStartY = pos.y;
                        originalX = index === 0 ? shapeFromX : shapeToX;
                        originalY = index === 0 ? shapeFromY : shapeToY;
                    }

                    onReleased: {
                        baseShapeRoot.isResizing = false;
                        controller.endEdit();
                    }

                    onPositionChanged: (mouse) => {
                        if (pressed) {
                            let pos = mapToItem(baseShapeRoot, mouse.x, mouse.y);
                            let dx = pos.x - dragStartX;
                            let dy = pos.y - dragStartY;
                            let nx = originalX + dx;
                            let ny = originalY + dy;
                            if (index === 0) {
                                lineGeometryChanged(nx, ny, shapeToX, shapeToY);
                            } else {
                                lineGeometryChanged(shapeFromX, shapeFromY, nx, ny);
                            }
                        }
                    }
                }
            }
        }
    }
}
