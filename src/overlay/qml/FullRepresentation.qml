import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls

Item {
    id: fullRoot

    implicitWidth: theme.gridUnit * 24
    implicitHeight: theme.gridUnit * 30
    Layout.minimumWidth: 400
    Layout.minimumHeight: 520
    Layout.preferredWidth: 432
    Layout.preferredHeight: 550

    QtObject {
        id: theme
        readonly property int gridUnit: 18
        readonly property int smallSpacing: 4
        readonly property int largeSpacing: 8
        readonly property int iconSmall: 16
        readonly property int iconMedium: 24
        readonly property int iconLarge: 32
        readonly property color highlightColor: palette.highlight
        readonly property color disabledTextColor: Qt.alpha(palette.text, 0.5)
        readonly property color backgroundColor: palette.base
        readonly property color hoverColor: palette.midlight
        readonly property color warningColor: "#f39c12"
    }

    readonly property var backend: controller

    function cap(key) { return key.charAt(0).toUpperCase() + key.slice(1) }

    // Read side of the routing below: the selected shape's value if there is one, else
    // the matching default. Both are read eagerly on purpose — the selected* properties
    // notify on selectionChanged only, so a binding that touched just one of them would
    // go stale when a default is written with nothing selected.
    function get(key) {
        const selected = backend["selected" + cap(key)]
        const fallback = backend["default" + cap(key)]
        return backend.hasSelection ? selected : fallback
    }

    // Route a property change: to the selected shape if there is one, else to the
    // matching default. Every shape key has a default<Key> property, so one function
    // covers all of them.
    function set(key, value) {
        if (backend.hasSelection) {
            let props = {}
            props[key] = value
            backend.updateProperties(props)
        } else {
            backend["default" + cap(key)] = value
        }
    }

    // Track the currently selected tool name for draw mode
    property string activeDrawTool: "freehand"
    property bool isFillableActive: {
        if (backend.hasSelection) {
            let t = backend.selectedType.toLowerCase();
            return t === "rectangle" || t === "ellipse";
        }
        return activeDrawTool === "rectangle" || activeDrawTool === "ellipse";
    }

    property bool isTextActive: {
        if (backend.hasSelection) {
            return backend.selectedType.toLowerCase() === "text";
        }
        return activeDrawTool === "text";
    }

    // Handled by whoever hosts this menu (TrayPopup), which owns the picker panel — a
    // QtQuick.Dialogs ColorDialog is an xdg-desktop-portal dialog and opens fullscreen
    // on the primary output, which is never where the popup is.
    signal customColorRequested(string current, bool isFill)

    function applyCustomColor(c, isFill) {
        set(isFill ? "fillColor" : "color", c.toString())
    }

    Controls.ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: theme.smallSpacing

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: theme.smallSpacing

                Text {
                    text: "Scribbleway"
                    font.pixelSize: 16
                    font.bold: true
                    color: palette.text
                    Layout.fillWidth: true
                }
            }

            // Running View
            ColumnLayout {
                Layout.fillWidth: true
                spacing: theme.smallSpacing

                // Mode Toggle Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.smallSpacing

                    Controls.Button {
                        icon.source: "qrc:/icons/draw-freehand.svg"
                        text: "Draw"
                        checkable: true
                        checked: backend.activeTool !== "select"
                        Layout.fillWidth: true
                        onClicked: {
                            if (backend.activeTool === "select") {
                                backend.setActiveTool(fullRoot.activeDrawTool)
                            }
                        }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/edit-select.svg"
                        text: "Select"
                        checkable: true
                        checked: backend.activeTool === "select"
                        Layout.fillWidth: true
                        onClicked: {
                            backend.setActiveTool("select")
                        }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/edit-undo.svg"
                        text: "Undo"
                        Layout.fillWidth: true
                        onClicked: { backend.undo() }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/edit-redo.svg"
                        text: "Redo"
                        Layout.fillWidth: true
                        onClicked: { backend.redo() }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/edit-clear.svg"
                        text: "Clear All"
                        Layout.fillWidth: true
                        onClicked: { backend.clear() }
                    }
                }

                // Section Header
                Controls.Label {
                    text: backend.hasSelection ? "Edit Selection (" + backend.selectedType + ")" : "Default Properties & Tool"
                    font.bold: true
                    font.pixelSize: 11
                    color: theme.highlightColor
                }

                // Selection Management Row
                RowLayout {
                    Layout.fillWidth: true
                    visible: backend.hasSelection
                    spacing: theme.smallSpacing

                    Controls.Button {
                        icon.source: "qrc:/icons/edit-delete.svg"
                        text: "Delete"
                        Layout.fillWidth: true
                        onClicked: { backend.deleteSelected() }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/go-up.svg"
                        text: "Raise"
                        onClicked: { backend.raiseSelected() }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/go-down.svg"
                        text: "Lower"
                        onClicked: { backend.lowerSelected() }
                    }
                }

                // Active Tool & Target Screen Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.smallSpacing

                    Controls.Label {
                        text: "Active Tool:"
                    }

                    Controls.ComboBox {
                        // In-window dropdown: a Popup.Window one is a separate Wayland
                        // surface, and the tray popup dismisses itself on focus loss.
                        Component.onCompleted: popup.popupType = Controls.Popup.Item
                        Layout.fillWidth: true
                        model: ["Freehand", "Arrow", "Rectangle", "Ellipse", "Line", "Text"]
                        currentIndex: {
                            let tool = backend.activeTool.toLowerCase()
                            if (tool === "arrow") return 1
                            if (tool === "rectangle") return 2
                            if (tool === "ellipse") return 3
                            if (tool === "line") return 4
                            if (tool === "text") return 5
                            return 0
                        }
                        onActivated: {
                            let tools = ["freehand", "arrow", "rectangle", "ellipse", "line", "text"]
                            let selected = tools[index]
                            fullRoot.activeDrawTool = selected
                            backend.setActiveTool(selected)
                        }
                    }

                    Controls.Label {
                        text: "Target Screen:"
                    }

                    Controls.ComboBox {
                        // In-window dropdown: a Popup.Window one is a separate Wayland
                        // surface, and the tray popup dismisses itself on focus loss.
                        Component.onCompleted: popup.popupType = Controls.Popup.Item
                        Layout.fillWidth: true
                        model: backend.screenNames
                        currentIndex: {
                            let idx = backend.screenNames.indexOf(backend.targetScreen)
                            return idx >= 0 ? idx : 0
                        }
                        onActivated: {
                            backend.setTargetScreen(currentValue)
                        }
                    }
                }

                // Color Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.smallSpacing

                    Controls.Label {
                        text: "Color:"
                        Layout.alignment: Qt.AlignVCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.smallSpacing

                        property string activeColor: fullRoot.get("color")

                        Repeater {
                            model: backend.presetColors

                            Rectangle {
                                width: theme.gridUnit * 1.5
                                height: theme.gridUnit * 1.5
                                radius: 4
                                color: modelData
                                border.width: parent.activeColor === modelData ? 2 : 1
                                border.color: parent.activeColor === modelData ? theme.highlightColor : "gray"

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: { fullRoot.set("color", modelData) }
                                }
                            }
                        }

                        Controls.Button {
                            icon.source: "qrc:/icons/color-picker.svg"
                            text: "Custom"
                            onClicked: fullRoot.customColorRequested(parent.activeColor, false)
                        }
                    }
                }

                // Width Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.smallSpacing

                    Controls.Label {
                        text: "Width:"
                        width: theme.gridUnit * 3
                    }

                    Controls.Slider {
                        Layout.fillWidth: true
                        from: 1
                        to: 15
                        stepSize: 1
                        value: fullRoot.get("strokeWidth")
                        onMoved: { fullRoot.set("strokeWidth", value) }
                    }

                    Controls.Label {
                        text: Math.round(fullRoot.get("strokeWidth")) + "px"
                    }
                }

                // Opacity Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.smallSpacing

                    Controls.Label {
                        text: "Opacity:"
                        width: theme.gridUnit * 3
                    }

                    Controls.Slider {
                        Layout.fillWidth: true
                        from: 0.1
                        to: 1.0
                        stepSize: 0.05
                        value: fullRoot.get("opacity")
                        onMoved: { fullRoot.set("opacity", value) }
                    }

                    Controls.Label {
                        text: Math.round(fullRoot.get("opacity") * 100) + "%"
                    }
                }

                // Fill Row (Rectangle / Ellipse)
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: fullRoot.isFillableActive
                    spacing: theme.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.smallSpacing

                        Controls.Label {
                            text: "Fill:"
                            Layout.alignment: Qt.AlignVCenter
                            width: theme.gridUnit * 3
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.smallSpacing

                                property string activeFill: fullRoot.get("fillColor")

                            Rectangle {
                                width: theme.gridUnit * 1.5
                                height: theme.gridUnit * 1.5
                                radius: 4
                                color: theme.backgroundColor
                                border.width: parent.activeFill === "transparent" ? 2 : 1
                                border.color: parent.activeFill === "transparent" ? theme.highlightColor : "gray"

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: parent.width - 6
                                    height: 2
                                    color: "red"
                                    rotation: 45
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: { fullRoot.set("fillColor", "transparent") }
                                }
                            }

                            Repeater {
                                model: backend.presetColors

                                Rectangle {
                                    width: theme.gridUnit * 1.5
                                    height: theme.gridUnit * 1.5
                                    radius: 4
                                    color: modelData
                                    border.width: parent.activeFill === modelData ? 2 : 1
                                    border.color: parent.activeFill === modelData ? theme.highlightColor : "gray"

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: { fullRoot.set("fillColor", modelData) }
                                    }
                                }
                            }

                            Controls.Button {
                                icon.source: "qrc:/icons/color-picker.svg"
                                text: "Custom"
                                onClicked: fullRoot.customColorRequested(
                                    parent.activeFill !== "transparent" ? parent.activeFill : "#e63946", true)
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.smallSpacing

                        Controls.Label {
                            text: "Fill Op:"
                            width: theme.gridUnit * 3
                        }

                        Controls.Slider {
                            Layout.fillWidth: true
                            from: 0.0
                            to: 1.0
                            stepSize: 0.05
                            value: fullRoot.get("fillOpacity")
                            onMoved: { fullRoot.set("fillOpacity", value) }
                        }

                        Controls.Label {
                            text: Math.round(fullRoot.get("fillOpacity") * 100) + "%"
                        }
                    }
                }

                // Glow Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.smallSpacing

                    Controls.Label {
                        text: "Glow:"
                        width: theme.gridUnit * 3
                    }

                    Controls.Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: 30
                        stepSize: 1
                        value: fullRoot.get("glow")
                        onMoved: { fullRoot.set("glow", value) }
                    }

                    Controls.Label {
                        text: Math.round(fullRoot.get("glow")) + "px"
                    }
                }

                // Roughness / Radius / Smoothing Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.smallSpacing

                    Controls.Label {
                        text: "Smoothing:"
                        width: theme.gridUnit * 4
                    }

                    Controls.Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: 3
                        stepSize: 1
                        value: fullRoot.get("freehandSmoothing")
                        onMoved: { fullRoot.set("freehandSmoothing", value) }
                    }

                    Controls.Label {
                        text: {
                            let v = Math.round(fullRoot.get("freehandSmoothing"))
                            if (v <= 0) return "Off"
                            if (v === 1) return "Low"
                            if (v === 2) return "Med"
                            return "High"
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.smallSpacing

                    Controls.Label {
                        text: "Roughness:"
                        width: theme.gridUnit * 4
                    }

                    Controls.ComboBox {
                        // In-window dropdown: a Popup.Window one is a separate Wayland
                        // surface, and the tray popup dismisses itself on focus loss.
                        Component.onCompleted: popup.popupType = Controls.Popup.Item
                        Layout.fillWidth: true
                        model: ["Neat (0)", "Artist (1)", "Cartoon (2)"]
                        currentIndex: Math.min(2, Math.max(0, Math.round(fullRoot.get("roughness"))))
                        onActivated: { fullRoot.set("roughness", index) }
                    }

                    Controls.Label {
                        text: "Radius:"
                        width: theme.gridUnit * 3
                    }

                    Controls.Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: 50
                        stepSize: 1
                        value: fullRoot.get("borderRadius")
                        onMoved: { fullRoot.set("borderRadius", value) }
                    }

                    Controls.Label {
                        text: Math.round(fullRoot.get("borderRadius")) + "px"
                    }
                }

                // Font Section (Text tool)
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: fullRoot.isTextActive
                    spacing: theme.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        Controls.Label {
                            text: "Font:"
                            width: theme.gridUnit * 3
                        }

                        Controls.ComboBox {
                            Component.onCompleted: popup.popupType = Controls.Popup.Item
                            Layout.fillWidth: true
                            model: ["sans-serif", "serif", "monospace", "Comic Sans MS"]
                            currentIndex: {
                                let f = fullRoot.get("fontFamily")
                                let idx = model.indexOf(f)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: { fullRoot.set("fontFamily", currentValue) }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Controls.Label {
                            text: "Font Size:"
                            width: theme.gridUnit * 3
                        }

                        Controls.Slider {
                            Layout.fillWidth: true
                            from: 10
                            to: 72
                            stepSize: 1
                            value: fullRoot.get("fontSize")
                            onMoved: { fullRoot.set("fontSize", value) }
                        }

                        Controls.Label {
                            text: Math.round(fullRoot.get("fontSize")) + "px"
                        }
                    }
                }

                // Shape List Manager
                Controls.Label {
                    text: "Drawn Shapes Manager"
                    font.bold: true
                    font.pixelSize: 11
                    color: theme.highlightColor
                }

                Controls.ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: theme.gridUnit * 6
                    clip: true

                    ListView {
                        id: shapesListView
                        objectName: "shapesListView"
                        // The shapes model itself, not a parallel metadata copy of it:
                        // it already carries type/selected/locked and signals its own
                        // changes.
                        model: backend.shapesModel

                        delegate: Controls.ItemDelegate {
                            id: shapeRow
                            required property int index
                            required property string type
                            // var, not bool: these keys are optional on a shape map, and
                            // an absent one arrives as undefined.
                            required property var selected
                            required property var locked

                            width: shapesListView.width
                            height: theme.gridUnit * 2

                            background: Rectangle {
                                color: !!shapeRow.selected ? theme.highlightColor
                                                         : (shapeRow.hovered ? theme.hoverColor : "transparent")
                                opacity: shapeRow.selected ? 0.3 : 1.0
                                radius: 4
                            }

                            contentItem: RowLayout {
                                spacing: theme.smallSpacing

                                Image {
                                    source: {
                                        switch (shapeRow.type.toLowerCase()) {
                                            case "rectangle": return "qrc:/icons/draw-rectangle.svg";
                                            case "ellipse": return "qrc:/icons/draw-ellipse.svg";
                                            case "arrow": return "qrc:/icons/draw-arrow.svg";
                                            case "line": return "qrc:/icons/draw-line.svg";
                                            case "text": return "qrc:/icons/draw-text.svg";
                                            default: return "qrc:/icons/draw-freehand.svg";
                                        }
                                    }
                                    width: theme.iconSmall
                                    height: theme.iconSmall
                                    fillMode: Image.PreserveAspectFit
                                }

                                Controls.Label {
                                    text: shapeRow.type.charAt(0).toUpperCase() + shapeRow.type.slice(1)
                                          + " " + (shapeRow.index + 1)
                                    font.bold: !!shapeRow.selected
                                    Layout.fillWidth: true
                                }

                                Controls.ToolButton {
                                    icon.source: !!shapeRow.locked ? "qrc:/icons/object-locked.svg"
                                                                 : "qrc:/icons/object-unlocked.svg"
                                    onClicked: backend.setShapeLocked(shapeRow.index, !shapeRow.locked)
                                }

                                Controls.ToolButton {
                                    icon.source: "qrc:/icons/edit-delete.svg"
                                    onClicked: backend.deleteShape(shapeRow.index)
                                }
                            }

                            onClicked: backend.selectShape(shapeRow.index)
                        }

                        Controls.Label {
                            anchors.centerIn: parent
                            text: "No annotations on screen"
                            color: theme.disabledTextColor
                            visible: shapesListView.count === 0
                        }
                    }
                }
            }
        }
    }
}
