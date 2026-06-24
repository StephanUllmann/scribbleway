import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls
import QtQuick.Shapes

Window {
    id: canvasWindow
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint
    width: Screen.width
    height: Screen.height

    // Active drawing states
    property string activeDrawTool: "" // "freehand", "rectangle", "ellipse", "line", "arrow", "text" or "" (passive)
    onActiveDrawToolChanged: {
        if (controller.activeTool !== activeDrawTool) {
            controller.activeTool = activeDrawTool;
        }
    }
    property bool isDrawing: false
    property point drawStartPoint: Qt.point(0, 0)
    property var activePoints: []
    
    // Live preview properties
    property real previewX: 0
    property real previewY: 0
    property real previewW: 0
    property real previewH: 0



    // Text editing index
    property int editingShapeIndex: -1
    property bool isUpdatingInputRegion: false
    property point lastMousePos: Qt.point(width / 2, height / 2)

    // Selection frame properties
    property bool isSelectingFrame: false
    property point selectStartPoint: Qt.point(0, 0)
    property real selectX: 0
    property real selectY: 0
    property real selectW: 0
    property real selectH: 0

    // Connections to C++ controller signals
    Connections {
        target: controller

        function onActiveToolChanged() {
            let tool = controller.activeTool;
            if (textEditor.visible) {
                textEditor.commitText();
            }
            if (canvasWindow.activeDrawTool !== tool) {
                if (canvasWindow.isDrawing) {
                    canvasWindow.abortShape();
                }
                canvasWindow.activeDrawTool = tool;
                canvasWindow.requestInputRegionUpdate();
            }
        }
        
        function onStartDrawingGesture(tool) {
            if (canvasWindow.isDrawing) {
                canvasWindow.abortShape();
            }
            if (canvasWindow.activeDrawTool === tool) {
                canvasWindow.activeDrawTool = "";
            } else {
                canvasWindow.activeDrawTool = tool;
            }
            canvasWindow.requestInputRegionUpdate();
        }


        function onEnterSelectModeRequested() {
            // Exit any active drawing
            if (canvasWindow.isDrawing) {
                canvasWindow.abortShape();
            }
            if (textEditor.visible) {
                textEditor.commitText();
            }
            canvasWindow.activeDrawTool = "";
            // Update input mask to include all shape bounding boxes
            canvasWindow.requestInputRegionUpdate();
        }

        function onEnterPassthroughModeRequested() {
            // Exit any active drawing
            if (canvasWindow.isDrawing) {
                canvasWindow.abortShape();
            }
            if (textEditor.visible) {
                textEditor.commitText();
            }
            canvasWindow.activeDrawTool = "";
            controller.setSelectedIndex(-1);
            canvasWindow.requestInputRegionUpdate();
        }

        function onModeChanged(mode) {
            canvasWindow.requestInputRegionUpdate();
        }

        function onSelectionChanged() {
            canvasWindow.requestInputRegionUpdate();
        }
    }



    // Abort shape without writing to model
    function abortShape() {
        if (!isDrawing) return;
        isDrawing = false;
        activePoints = [];
        previewW = 0;
        previewH = 0;
    }

    // Finalize shape and write to model
    function finalizeShape() {
        if (!isDrawing) return;
        isDrawing = false;



        let shape = {
            "type": activeDrawTool,
            "color": controller.defaultColor,
            "strokeWidth": controller.defaultStrokeWidth,
            "opacity": controller.defaultOpacity,
            "selected": false,
            "locked": false
        };

        if (activeDrawTool === "freehand") {
            // Only add if we have at least 2 points
            if (activePoints.length >= 2) {
                shape["points"] = activePoints;
                controller.addShape(shape);
            }
        } else if (activeDrawTool === "line" || activeDrawTool === "arrow") {
            // Only add if line has length
            let dx = previewW;
            let dy = previewH;
            if (Math.abs(dx) > 2 || Math.abs(dy) > 2) {
                shape["fromX"] = drawStartPoint.x;
                shape["fromY"] = drawStartPoint.y;
                shape["toX"] = drawStartPoint.x + dx;
                shape["toY"] = drawStartPoint.y + dy;
                controller.addShape(shape);
            }
        } else if (activeDrawTool === "text") {
            shape["x"] = drawStartPoint.x;
            shape["y"] = drawStartPoint.y;
            shape["width"] = 100;
            shape["height"] = 40;
            shape["text"] = "";
            shape["fontFamily"] = controller.defaultFontFamily;
            shape["fontSize"] = controller.defaultFontSize;
            
            controller.addShape(shape);
            // Open editor on the newly created text shape (last item)
            startTextEditing(controller.shapesModel.rowCount() - 1);
        } else {
            // Rectangle or Ellipse
            if (previewW > 4 && previewH > 4) {
                shape["x"] = previewX;
                shape["y"] = previewY;
                shape["width"] = previewW;
                shape["height"] = previewH;
                if (activeDrawTool === "rectangle") {
                    shape["borderRadius"] = controller.defaultBorderRadius;
                }
                controller.addShape(shape);
            }
        }

        activePoints = [];
        previewW = 0;
        previewH = 0;
    }

    // Dynamic input region updates based on active drawing/selection modes
    function requestInputRegionUpdate() {
        let isPassthrough = (controller.currentMode === "passthrough" && activeDrawTool === "");
        let needsKeyboard = !isPassthrough || textEditor.visible;

        controller.setKeyboardInteractivity(needsKeyboard);
        if (needsKeyboard) {
            canvasWindow.requestActivate();
        }

        if (isPassthrough && !textEditor.visible) {
            controller.updateInputMask([]);
        } else if (isPassthrough && textEditor.visible) {
            controller.updateInputMask([Qt.rect(textEditor.x - 5, textEditor.y - 5, textEditor.width + 10, textEditor.height + 10)]);
        } else {
            controller.updateInputMask([Qt.rect(0, 0, canvasWindow.width, canvasWindow.height)]);
        }
    }

    // Recalculate input region when window finishes resizing or shapes count changes
    onWidthChanged: requestInputRegionUpdate()
    onHeightChanged: requestInputRegionUpdate()

    // Mouse hover tracker to capture cursor position under Wayland
    MouseArea {
        id: hoverTracker
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        onPositionChanged: (mouse) => {
            canvasWindow.lastMousePos = Qt.point(mouse.x, mouse.y)
        }
    }

    // Shapes Repeater (Renders all shapes)
    Repeater {
        id: shapeRepeater
        model: controller.shapesModel
        delegate: Loader {
            id: loader
            anchors.fill: parent
            source: {
                switch(model.type.toLowerCase()) {
                    case "rectangle": return "shapes/RectangleShape.qml";
                    case "ellipse": return "shapes/EllipseShape.qml";
                    case "line": return "shapes/LineShape.qml";
                    case "arrow": return "shapes/ArrowShape.qml";
                    case "freehand": return "shapes/FreehandShape.qml";
                    case "text": return "shapes/TextShape.qml";
                }
                return "";
            }
            onLoaded: {
                if (item) {
                    item.index = index;
                }
                canvasWindow.requestInputRegionUpdate();
            }
        }
    }

    // Active Drawing Layer (only captures gestures when activeDrawTool is set)
    MouseArea {
        id: drawMouseArea
        anchors.fill: parent
        enabled: activeDrawTool !== "" && !textEditor.visible
        cursorShape: Qt.CrossCursor

        onPressed: (mouse) => {
            isDrawing = true;
            drawStartPoint = Qt.point(mouse.x, mouse.y);
            
            if (activeDrawTool === "freehand") {
                activePoints = [drawStartPoint];
            } else {
                previewX = mouse.x;
                previewY = mouse.y;
                previewW = 0;
                previewH = 0;
            }
        }

        onPositionChanged: (mouse) => {
            if (!isDrawing) return;

            if (activeDrawTool === "freehand") {
                // Batch points for rendering performance
                let pts = activePoints;
                pts.push(Qt.point(mouse.x, mouse.y));
                activePoints = pts;
                freehandPreviewPath.path = activePoints;
            } else {
                // For lines/arrows, width and height represent delta x/y
                if (activeDrawTool === "line" || activeDrawTool === "arrow") {
                    previewW = mouse.x - drawStartPoint.x;
                    previewH = mouse.y - drawStartPoint.y;
                } else {
                    // Rect or Ellipse
                    previewX = Math.min(mouse.x, drawStartPoint.x);
                    previewY = Math.min(mouse.y, drawStartPoint.y);
                    previewW = Math.abs(mouse.x - drawStartPoint.x);
                    previewH = Math.abs(mouse.y - drawStartPoint.y);
                }
            }
        }

        onReleased: {
            finalizeShape();
        }
    }

    // --- LIVE PREVIEW SHAPES ---
    
    // Freehand Live Preview
    Shape {
        anchors.fill: parent
        visible: isDrawing && activeDrawTool === "freehand"
        opacity: controller.defaultOpacity
        
        ShapePath {
            strokeColor: controller.defaultColor
            strokeWidth: controller.defaultStrokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            
            PathPolyline {
                id: freehandPreviewPath
                path: []
            }
        }
    }

    // Rectangle Live Preview
    Rectangle {
        visible: isDrawing && activeDrawTool === "rectangle"
        x: previewX
        y: previewY
        width: previewW
        height: previewH
        opacity: controller.defaultOpacity
        border.color: controller.defaultColor
        border.width: controller.defaultStrokeWidth
        color: {
            let c = Qt.color(controller.defaultColor);
            return Qt.rgba(c.r, c.g, c.b, 0.12);
        }
        radius: controller.defaultBorderRadius
    }

    // Ellipse Live Preview
    Shape {
        anchors.fill: parent
        visible: isDrawing && activeDrawTool === "ellipse"
        opacity: controller.defaultOpacity

        ShapePath {
            strokeColor: controller.defaultColor
            strokeWidth: controller.defaultStrokeWidth
            fillColor: {
                let c = Qt.color(controller.defaultColor);
                return Qt.rgba(c.r, c.g, c.b, 0.12);
            }

            startX: previewX + previewW / 2
            startY: previewY

            PathArc {
                x: previewX + previewW / 2
                y: previewY + previewH
                radiusX: previewW / 2
                radiusY: previewH / 2
                useLargeArc: false
                direction: PathArc.Clockwise
            }

            PathArc {
                x: previewX + previewW / 2
                y: previewY
                radiusX: previewW / 2
                radiusY: previewH / 2
                useLargeArc: false
                direction: PathArc.Clockwise
            }
        }
    }

    // Line Live Preview
    Shape {
        anchors.fill: parent
        visible: isDrawing && activeDrawTool === "line"
        opacity: controller.defaultOpacity

        ShapePath {
            strokeColor: controller.defaultColor
            strokeWidth: controller.defaultStrokeWidth
            fillColor: "transparent"

            startX: drawStartPoint.x
            startY: drawStartPoint.y
            PathLine {
                x: drawStartPoint.x + previewW
                y: drawStartPoint.y + previewH
            }
        }
    }

    // Arrow Live Preview
    Shape {
        id: arrowPreviewShape
        anchors.fill: parent
        visible: isDrawing && activeDrawTool === "arrow"
        opacity: controller.defaultOpacity
        preferredRendererType: Shape.CurveRenderer

        // Arrow trigonometry calculations
        property real angle: Math.atan2(previewH, previewW)
        property real len: 10 + controller.defaultStrokeWidth * 1.5
        property real halfAng: Math.PI / 6
        property real tx: drawStartPoint.x + previewW
        property real ty: drawStartPoint.y + previewH

        // Calculate midpoint at the bottom/base of the arrowhead
        property real lineLength: Math.sqrt(Math.pow(previewW, 2) + Math.pow(previewH, 2))
        property real stemLength: Math.max(0, lineLength - len * Math.cos(halfAng))
        property real baseX: drawStartPoint.x + stemLength * Math.cos(angle)
        property real baseY: drawStartPoint.y + stemLength * Math.sin(angle)

        ShapePath {
            strokeColor: controller.defaultColor
            strokeWidth: controller.defaultStrokeWidth
            fillColor: "transparent"

            startX: drawStartPoint.x
            startY: drawStartPoint.y
            PathLine { x: arrowPreviewShape.baseX; y: arrowPreviewShape.baseY }
        }

        ShapePath {
            strokeColor: "transparent"
            fillColor: controller.defaultColor

            startX: arrowPreviewShape.tx
            startY: arrowPreviewShape.ty
            PathLine {
                x: arrowPreviewShape.tx - arrowPreviewShape.len * Math.cos(arrowPreviewShape.angle - arrowPreviewShape.halfAng)
                y: arrowPreviewShape.ty - arrowPreviewShape.len * Math.sin(arrowPreviewShape.angle - arrowPreviewShape.halfAng)
            }
            PathLine {
                x: arrowPreviewShape.tx - arrowPreviewShape.len * Math.cos(arrowPreviewShape.angle + arrowPreviewShape.halfAng)
                y: arrowPreviewShape.ty - arrowPreviewShape.len * Math.sin(arrowPreviewShape.angle + arrowPreviewShape.halfAng)
            }
            PathLine { x: arrowPreviewShape.tx; y: arrowPreviewShape.ty }
        }
    }

    // --- SELECTION FRAME UI ---
    Rectangle {
        id: selectionFrameRect
        visible: isSelectingFrame
        x: selectX
        y: selectY
        width: selectW
        height: selectH
        color: "#1a3b82f6" // 10% opacity blue
        border.color: "#3b82f6"
        border.width: 1
    }

    // Hidden text measurer to calculate natural text size without binding loops
    Text {
        id: textMeasurer
        visible: false
        font.family: textEditor.font.family
        font.pixelSize: textEditor.font.pixelSize
        text: textEditor.text
    }

    Controls.TextArea {
        id: textEditor
        visible: false
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        
        property real maxWidth: canvasWindow.width - x - 20
        width: Math.min(maxWidth, Math.max(150, textMeasurer.implicitWidth + leftPadding + rightPadding + 20))
        height: Math.max(40, contentHeight + topPadding + bottomPadding + 10)
        
        background: Rectangle {
            color: Qt.rgba(1, 1, 1, 0.45)
            border.color: "#3b82f6"
            border.width: 1.5
            radius: 4
        }
        
        font.family: {
            if (editingShapeIndex !== -1 && editingShapeIndex < controller.shapesModel.rowCount()) {
                let shape = controller.getShape(editingShapeIndex);
                if (shape && shape.fontFamily) return shape.fontFamily;
            }
            return controller.defaultFontFamily;
        }
        font.pixelSize: {
            if (editingShapeIndex !== -1 && editingShapeIndex < controller.shapesModel.rowCount()) {
                let shape = controller.getShape(editingShapeIndex);
                if (shape && shape.fontSize) return shape.fontSize;
            }
            return controller.defaultFontSize;
        }
        color: "black"

        onWidthChanged: canvasWindow.requestInputRegionUpdate()
        onHeightChanged: canvasWindow.requestInputRegionUpdate()

        onVisibleChanged: {
            if (visible) {
                forceActiveFocus();
                selectAll();
            }
            canvasWindow.requestInputRegionUpdate();
        }

        // Finalize text input when clicking outside or pressing Escape
        function commitText() {
            if (editingShapeIndex !== -1) {

                
                let val = text.trim();
                if (val === "") {
                    // Delete empty text shape
                    controller.deleteShape(editingShapeIndex);
                } else {
                    controller.updateShape(editingShapeIndex, { "text": val });
                }
            }
            visible = false;
            editingShapeIndex = -1;
            canvasWindow.requestInputRegionUpdate();
        }

        Keys.onEscapePressed: {
            canvasWindow.cancelInteraction();
        }

        // On Tab or Shift+Enter, complete text. Let enter key create new lines.
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Return && !(event.modifiers & Qt.ShiftModifier)) {
                commitText();
                event.accepted = true;
            } else if (event.key === Qt.Key_Tab) {
                commitText();
                event.accepted = true;
            }
        }
    }

    function startTextEditing(shapeIndex) {
        let shape = controller.getShape(shapeIndex);
        if (!shape) return;

        editingShapeIndex = shapeIndex;
        controller.setSelectedIndex(shapeIndex);

        // Position text editor exactly over the text shape bounds
        textEditor.x = shape.x;
        textEditor.y = shape.y;
        textEditor.text = shape.text || "";
        textEditor.visible = true;

        canvasWindow.requestInputRegionUpdate();
    }

    function cancelInteraction() {
        if (textEditor.visible) {
            textEditor.commitText();
        }
        controller.enterPassthroughMode();
    }

    // Cancel selection or focus when Escape is pressed
    Shortcut {
        sequence: "Escape"
        onActivated: {
            canvasWindow.cancelInteraction();
        }
    }

    Shortcut {
        sequence: "Delete"
        enabled: !textEditor.visible
        onActivated: {
            controller.deleteSelected();
        }
    }

    Shortcut {
        sequence: "Backspace"
        enabled: !textEditor.visible
        onActivated: {
            controller.deleteSelected();
        }
    }

    Shortcut {
        sequence: StandardKey.Copy
        enabled: !textEditor.visible
        onActivated: {
            controller.copySelected();
        }
    }

    Shortcut {
        sequence: StandardKey.Paste
        enabled: !textEditor.visible
        onActivated: {
            controller.pasteFromClipboard(canvasWindow.lastMousePos.x, canvasWindow.lastMousePos.y);
        }
    }


    // Background interaction: handles click-to-deselect, text commit, and drag-selection frame
    MouseArea {
        anchors.fill: parent
        z: -1 // Behind shapes and draw capture layers
        enabled: controller.currentMode === "select" || controller.selectedIndex !== -1 || textEditor.visible

        onPressed: (mouse) => {
            if (textEditor.visible) {
                textEditor.commitText();
            } else if (controller.currentMode === "select") {
                isSelectingFrame = true;
                selectStartPoint = Qt.point(mouse.x, mouse.y);
                selectX = mouse.x;
                selectY = mouse.y;
                selectW = 0;
                selectH = 0;
                controller.beginDragSelection(mouse.modifiers & Qt.ShiftModifier);
                controller.selectShapesInRect(selectX, selectY, 0, 0, mouse.modifiers & Qt.ShiftModifier);
            } else {
                controller.setSelectedIndex(-1);
            }
        }

        onPositionChanged: (mouse) => {
            if (isSelectingFrame) {
                selectX = Math.min(mouse.x, selectStartPoint.x);
                selectY = Math.min(mouse.y, selectStartPoint.y);
                selectW = Math.abs(mouse.x - selectStartPoint.x);
                selectH = Math.abs(mouse.y - selectStartPoint.y);
                controller.selectShapesInRect(selectX, selectY, selectW, selectH, mouse.modifiers & Qt.ShiftModifier);
            }
        }

        onReleased: {
            if (isSelectingFrame) {
                isSelectingFrame = false;
                selectW = 0;
                selectH = 0;
            }
        }
    }
}
