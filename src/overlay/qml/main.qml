import QtQuick
import QtQml
import QtQuick.Window
import QtQuick.Controls as Controls
import QtQuick.Shapes
import "shapes"
import "shapes/RoughPathGenerator.js" as RoughPathGenerator

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
    
    // The gesture being drawn; draftShape() turns it into a Shape.
    property int drawSeed: 1
    property real previewX: 0
    property real previewY: 0
    property real previewW: 0
    property real previewH: 0



    // Text editing target: "standalone" edits a text shape, "attached" edits a container binding.
    property int editingShapeIndex: -1
    property string editingTextKind: ""
    property point lastMousePos: Qt.point(width / 2, height / 2)

    // Selection frame properties
    property bool isSelectingFrame: false
    property point selectStartPoint: Qt.point(0, 0)
    property real selectX: 0
    property real selectY: 0
    property real selectW: 0
    readonly property bool shortcutGuard: controller.currentMode !== "passthrough" && !textEditor.visible
    property real selectH: 0

    // Snap indicator state
    property int snapTargetIndex: -1
    property real snapPointX: 0
    property real snapPointY: 0
    property bool hasSnap: snapTargetIndex >= 0

    // Cursor feedback -------------------------------------------------
    // Maps draw tools to Qt.CursorShape. Most geometry tools share
    // CrossCursor; the floating ToolCursorBadge disambiguates them.
    function cursorForDrawTool(tool) {
        switch (tool) {
        case "text":
            return Qt.IBeamCursor;
        case "freehand":
        case "rectangle":
        case "ellipse":
        case "line":
        case "arrow":
            return Qt.CrossCursor;
        default:
            return Qt.ArrowCursor;
        }
    }

    readonly property bool showToolCursorBadge:
        activeDrawTool !== ""
        && controller.currentMode !== "passthrough"
        && !textEditor.visible

    readonly property bool showSelectToolBadge:
        activeDrawTool === ""
        && controller.currentMode === "select"
        && !textEditor.visible


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
            trayPopup.close();
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
        controller.setDraftShape({});
        activePoints = [];
        previewW = 0;
        previewH = 0;
    }

    // The Shape as it would be added right now. The Draft and finalizeShape() both come
    // from here, so what the user sees while dragging is what lands on release.
    function draftShape() {
        let shape = Object.assign(controller.styleForNewShape(activeDrawTool), {
            "type": activeDrawTool,
            "locked": false,
            "seed": drawSeed
        });
        if (activeDrawTool === "freehand") {
            shape["points"] = activePoints;
        } else if (activeDrawTool === "line" || activeDrawTool === "arrow") {
            // For lines/arrows, previewW and previewH are the delta from the start point.
            shape["fromX"] = drawStartPoint.x;
            shape["fromY"] = drawStartPoint.y;
            shape["toX"] = drawStartPoint.x + previewW;
            shape["toY"] = drawStartPoint.y + previewH;
        } else {
            shape["x"] = previewX;
            shape["y"] = previewY;
            shape["width"] = previewW;
            shape["height"] = previewH;
        }
        return shape;
    }

    // Finalize shape and write to model
    function finalizeShape() {
        if (!isDrawing) return;
        isDrawing = false;
        controller.setDraftShape({});

        let shape = draftShape();

        if (activeDrawTool === "freehand") {
            // Only add if we have at least 2 points
            if (activePoints.length >= 2) {
                shape["points"] = RoughPathGenerator.smoothFreehandPoints(activePoints, controller.style.freehandSmoothing);
                controller.addShape(shape);
            }
        } else if (activeDrawTool === "line" || activeDrawTool === "arrow") {
            // Only add if line has length
            if (Math.abs(previewW) > 2 || Math.abs(previewH) > 2) {
                // One edit group, so the arrow and its attachments undo in one step.
                controller.beginEdit();
                controller.addShape(shape);
                controller.createBindingsForShape(controller.shapesModel.rowCount() - 1);
                controller.endEdit();
            }
        } else if (activeDrawTool === "text") {
            shape["x"] = drawStartPoint.x;
            shape["y"] = drawStartPoint.y;
            shape["width"] = 100;
            shape["height"] = 40;
            shape["text"] = "";
            
            controller.addShape(shape);
            // Open editor on the newly created text shape (last item)
            startTextEditing(controller.shapesModel.rowCount() - 1);
        } else {
            // Rectangle or Ellipse
            if (previewW > 4 && previewH > 4) {
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

        overlayWindow.setToolWantsKeyboard(needsKeyboard);
        // Not while the tray popup is up: it dismisses on focus loss, and this fires on
        // every selection change — including the ones the popup itself just caused.
        if (needsKeyboard && !trayPopup.open) {
            canvasWindow.requestActivate();
        }

        if (isPassthrough && !textEditor.visible) {
            overlayWindow.setShapeRegion([]);
        } else if (isPassthrough && textEditor.visible) {
            overlayWindow.setShapeRegion([Qt.rect(textEditor.x - 5, textEditor.y - 5, textEditor.width + 10, textEditor.height + 10)]);
        } else {
            overlayWindow.setShapeRegion([Qt.rect(0, 0, canvasWindow.width, canvasWindow.height)]);
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
        // Only useful when drawMouseArea is disabled; when drawing,
        // drawMouseArea owns hover (see below).
        enabled: !(activeDrawTool !== "" && !textEditor.visible)
        onPositionChanged: (mouse) => {
            canvasWindow.lastMousePos = Qt.point(mouse.x, mouse.y)
        }
    }

    function shapeSource(type) {
        switch (type.toLowerCase()) {
            case "rectangle": return "shapes/RectangleShape.qml";
            case "ellipse": return "shapes/EllipseShape.qml";
            case "line": return "shapes/LineShape.qml";
            case "arrow": return "shapes/ArrowShape.qml";
            case "freehand": return "shapes/FreehandShape.qml";
            case "text": return "shapes/TextShape.qml";
        }
        return "";
    }

    // Shapes Repeater (Renders all shapes)
    Repeater {
        id: shapeRepeater
        model: controller.shapesModel
        delegate: Loader {
            id: loader
            anchors.fill: parent
            visible: !controller.shapesHidden
            source: canvasWindow.shapeSource(model.type)
            onLoaded: {
                if (item) {
                    // A binding: rows shift on delete, raise and lower without a new delegate.
                    item.index = Qt.binding(() => index);
                }
                canvasWindow.requestInputRegionUpdate();
            }
        }
    }

    // The Draft, drawn by the same delegates as the committed Shapes. It takes no input:
    // the draw layer above owns the gesture.
    Repeater {
        model: controller.draftModel
        delegate: Loader {
            anchors.fill: parent
            enabled: false
            source: canvasWindow.shapeSource(model.type)
            // Shows the live size label, the same one a resize shows.
            onLoaded: item.resizing = true
        }
    }

    // Active Drawing Layer (only captures gestures when activeDrawTool is set)
    MouseArea {
        id: drawMouseArea
        anchors.fill: parent
        // visible, not enabled: a disabled MouseArea still wins the cursor lookup, so this
        // full-window layer would hide every shape's handle and move cursor beneath it.
        visible: activeDrawTool !== "" && !textEditor.visible
        hoverEnabled: true
        cursorShape: canvasWindow.cursorForDrawTool(activeDrawTool)

        onPressed: (mouse) => {
            canvasWindow.lastMousePos = Qt.point(mouse.x, mouse.y);
            isDrawing = true;
            drawSeed = Math.floor(Math.random() * 1000000) + 1;
            drawStartPoint = Qt.point(mouse.x, mouse.y);
            snapTargetIndex = -1;
            
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
            canvasWindow.lastMousePos = Qt.point(mouse.x, mouse.y);

            if (!isDrawing) return;

            if (activeDrawTool === "freehand") {
                // Batch points for rendering performance
                let pts = activePoints;
                pts.push(Qt.point(mouse.x, mouse.y));
                activePoints = pts;
            } else {
                // For lines/arrows, width and height represent delta x/y
                if (activeDrawTool === "line" || activeDrawTool === "arrow") {
                    previewW = mouse.x - drawStartPoint.x;
                    previewH = mouse.y - drawStartPoint.y;

                    // Check snap for the endpoint being drawn (toX/toY)
                    let endSnap = controller.findSnapInfo(mouse.x, mouse.y);
                    // Also check snap for start point
                    let startSnap = controller.findSnapInfo(drawStartPoint.x, drawStartPoint.y);

                    // Show snap for the end point (more relevant during active draw)
                    if (endSnap.valid) {
                        snapTargetIndex = endSnap.targetIndex;
                        snapPointX = endSnap.snapX;
                        snapPointY = endSnap.snapY;
                    } else if (startSnap.valid) {
                        snapTargetIndex = startSnap.targetIndex;
                        snapPointX = startSnap.snapX;
                        snapPointY = startSnap.snapY;
                    } else {
                        snapTargetIndex = -1;
                    }
                } else {
                    // Rect or Ellipse
                    previewX = Math.min(mouse.x, drawStartPoint.x);
                    previewY = Math.min(mouse.y, drawStartPoint.y);
                    previewW = Math.abs(mouse.x - drawStartPoint.x);
                    previewH = Math.abs(mouse.y - drawStartPoint.y);
                }
            }
            // A text box opens its editor on release; it has nothing to show before that.
            if (activeDrawTool !== "text") {
                controller.setDraftShape(draftShape());
            }
        }

        onReleased: {
            snapTargetIndex = -1;
            finalizeShape();
        }
    }

    // Snap indicator dot
    Rectangle {
        visible: hasSnap && isDrawing && (activeDrawTool === "line" || activeDrawTool === "arrow")
        x: snapPointX - 5
        y: snapPointY - 5
        width: 10
        height: 10
        radius: 5
        color: "#3b82f6"
        border.color: "white"
        border.width: 1.5
        z: 999
    }

    // --- TOOL CURSOR BADGE (follows pointer while a draw tool is active) ---
    ToolCursorBadge {
        id: toolCursorBadge
        visible: canvasWindow.showToolCursorBadge
        tool: canvasWindow.activeDrawTool
        accent: controller.style.color
        z: 1000
        opacity: canvasWindow.isDrawing ? 0.55 : 0.92
        x: {
            let nx = canvasWindow.lastMousePos.x + 16;
            return Math.min(canvasWindow.width - toolCursorBadge.width - 4, Math.max(4, nx));
        }
        y: {
            let ny = canvasWindow.lastMousePos.y + 16;
            return Math.min(canvasWindow.height - toolCursorBadge.height - 4, Math.max(4, ny));
        }
    }

    // --- SELECTION TOOL BADGE (placed in upper left corner when selection tool is active) ---
    ToolCursorBadge {
        id: selectToolBadge
        visible: canvasWindow.showSelectToolBadge
        tool: "select"
        accent: controller.style.color
        z: 1000
        x: 16
        y: 16
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
        textFormat: Text.PlainText
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
        property real containerWidth: 0
        property real containerHeight: 0
        // Text in a shape is edited in place: the same box, centering and color as the
        // finished label, with no background, so only the cursor shows.
        readonly property bool inShape: editingTextKind === "attached"
        width: inShape ? containerWidth : Math.min(maxWidth, Math.max(150, textMeasurer.implicitWidth + leftPadding + rightPadding + 20))
        height: Math.max(inShape ? containerHeight : 40, contentHeight + topPadding + bottomPadding + (inShape ? 0 : 10))
        horizontalAlignment: inShape ? TextEdit.AlignHCenter : TextEdit.AlignLeft
        verticalAlignment: inShape ? TextEdit.AlignVCenter : TextEdit.AlignTop

        background: Rectangle {
            visible: !textEditor.inShape
            color: Qt.rgba(1, 1, 1, 0.45)
            border.color: "#3b82f6"
            border.width: 1.5
            radius: 4
        }
        
        font.family: {
            if (editingShapeIndex !== -1 && editingShapeIndex < controller.shapesModel.rowCount()) {
                let shape = controller.getShape(editingShapeIndex);
                if (shape && shape.fontFamily) return shape.fontFamily;
                if (shape && shape.attachedText && shape.attachedText.fontFamily) return shape.attachedText.fontFamily;
            }
            return controller.style.fontFamily;
        }
        font.pixelSize: {
            if (editingShapeIndex !== -1 && editingShapeIndex < controller.shapesModel.rowCount()) {
                let shape = controller.getShape(editingShapeIndex);
                if (shape && shape.fontSize) return shape.fontSize;
                if (shape && shape.attachedText && shape.attachedText.fontSize) return shape.attachedText.fontSize;
            }
            return controller.style.fontSize;
        }
        color: {
            if (inShape && editingShapeIndex !== -1 && editingShapeIndex < controller.shapesModel.rowCount()) {
                let shape = controller.getShape(editingShapeIndex);
                if (shape && shape.color) return shape.color;
            }
            return "black";
        }

        onWidthChanged: canvasWindow.requestInputRegionUpdate()
        onHeightChanged: canvasWindow.requestInputRegionUpdate()

        onVisibleChanged: {
            if (visible) {
                forceActiveFocus();
                selectAll();
            } else {
                // Hiding an item does not release its active focus: the editor stays
                // the window's activeFocusItem and keeps claiming plain keys through
                // ShortcutOverride, so every single-letter tool shortcut dies until
                // restart. Escape survives only because TextArea doesn't claim it —
                // which is exactly how this presents. Done here rather than in
                // commitText() so the mode handlers are covered by the same line.
                focus = false;
            }
            canvasWindow.requestInputRegionUpdate();
        }

        // Finalize text input when clicking outside or pressing Escape
        function commitText() {
            if (editingShapeIndex !== -1) {
                let val = text.trim();
                if (editingTextKind === "attached") {
                    if (val === "") {
                        controller.removeAttachedText(editingShapeIndex);
                    } else {
                        controller.setAttachedText(editingShapeIndex, val);
                    }
                } else {
                    if (val === "") {
                        controller.deleteShape(editingShapeIndex);
                    } else {
                        controller.updateShape(editingShapeIndex, { "text": val });
                    }
                }
            }
            visible = false;
            editingShapeIndex = -1;
            editingTextKind = "";
            canvasWindow.requestInputRegionUpdate();
        }

        // On Tab or Return (without Shift), complete text. Shift+Enter inserts a newline.
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
        editingTextKind = "standalone";
        controller.setSelectedIndex(shapeIndex);

        textEditor.x = shape.x;
        textEditor.y = shape.y;
        textEditor.containerWidth = 0;
        textEditor.text = shape.text || "";
        textEditor.visible = true;

        canvasWindow.requestInputRegionUpdate();
    }

    function startAttachedTextEditing(shapeIndex) {
        let shape = controller.getShape(shapeIndex);
        if (!shape) return;

        let attached = controller.ensureAttachedTextForShape(shapeIndex);
        if (!attached) return;

        editingShapeIndex = shapeIndex;
        editingTextKind = "attached";
        controller.setSelectedIndex(shapeIndex);

        const r = controller.attachedTextRect(shapeIndex);
        textEditor.x = r.x;
        textEditor.y = r.y;
        textEditor.containerWidth = r.width;
        textEditor.containerHeight = r.height;
        textEditor.text = attached.text || "";
        textEditor.visible = true;

        canvasWindow.requestInputRegionUpdate();
    }

    // A press anywhere the overlay can receive one is by construction outside the popup:
    // the popup's rect is carved out of the overlay's input region. A passive
    // PointHandler sees it without competing with the draw/select MouseAreas for the
    // grab, which beats bolting the same call onto all six press handlers.
    Item {
        anchors.fill: parent
        z: 1000

        PointHandler {
            enabled: trayPopup.open
            acceptedButtons: Qt.AllButtons
            onActiveChanged: if (active) trayPopup.close()
        }
    }

    // Every overlay hotkey comes from OverlayController's table (ids, defaults,
    // overrides and handlers). Instantiator rather than Repeater: Shortcut is not an Item.
    Instantiator {
        id: shortcutInstantiator
        objectName: "shortcutInstantiator"
        model: controller.localShortcuts
        delegate: Shortcut {
            required property var modelData
            sequences: modelData.sequences
            context: modelData.appWide ? Qt.ApplicationShortcut : Qt.WindowShortcut
            enabled: modelData.needs === "always"
                     || (canvasWindow.shortcutGuard
                         && (modelData.needs === ""
                             || (modelData.needs === "selection" && controller.hasSelection)
                             || controller.selection.type === modelData.needs))
            onActivated: controller.triggerShortcut(modelData.id, canvasWindow.lastMousePos)
        }
    }


    // Background interaction: handles click-to-deselect, text commit, and drag-selection frame
    MouseArea {
        anchors.fill: parent
        z: -1 // Behind shapes and draw capture layers
        enabled: controller.currentMode === "select" || controller.selectedIndex !== -1 || textEditor.visible
        hoverEnabled: true
        cursorShape: textEditor.visible ? Qt.ArrowCursor
                     : (isSelectingFrame ? Qt.CrossCursor : Qt.ArrowCursor)

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
            canvasWindow.lastMousePos = Qt.point(mouse.x, mouse.y);
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
