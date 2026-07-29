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

    // Read side of the same routing as the setters below. The selected* properties notify
    // on selectionChanged only, so a binding straight to them goes stale when a default is
    // written with nothing selected — pass both and let this pick, so the binding depends
    // on both signals.
    function pick(selected, fallback) {
        return backend.hasSelection ? selected : fallback
    }

    // Route property changes: to the selected shape if there is one, else to defaults.
    function setColor(color) {
        if (backend.hasSelection) backend.updateProperties({color: color})
        else backend.defaultColor = color
    }
    function setStrokeWidth(width) {
        if (backend.hasSelection) backend.updateProperties({strokeWidth: width})
        else backend.defaultStrokeWidth = width
    }
    function setOpacity(opacity) {
        if (backend.hasSelection) backend.updateProperties({opacity: opacity})
        else backend.defaultOpacity = opacity
    }
    function setFillColor(color) {
        if (backend.hasSelection) backend.updateProperties({fillColor: color})
        else backend.defaultFillColor = color
    }
    function setFillOpacity(opacity) {
        if (backend.hasSelection) backend.updateProperties({fillOpacity: opacity})
        else backend.defaultFillOpacity = opacity
    }
    function setGlow(glow) {
        if (backend.hasSelection) backend.updateProperties({glow: glow})
        else backend.defaultGlow = glow
    }
    function setFreehandSmoothing(level) {
        if (backend.hasSelection) backend.updateProperties({freehandSmoothing: level})
        else backend.defaultFreehandSmoothing = level
    }
    function setRoughness(roughness) {
        if (backend.hasSelection) backend.updateProperties({roughness: roughness})
        else backend.defaultRoughness = roughness
    }
    function setBorderRadius(radius) {
        if (backend.hasSelection) backend.updateProperties({borderRadius: radius})
        else backend.defaultBorderRadius = radius
    }
    function setFontFamily(family) {
        if (backend.hasSelection) backend.updateProperties({fontFamily: family})
        else backend.defaultFontFamily = family
    }
    function setFontSize(size) {
        if (backend.hasSelection) backend.updateProperties({fontSize: size})
        else backend.defaultFontSize = size
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
        if (isFill) setFillColor(c.toString())
        else setColor(c.toString())
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

                        property var colors: ["#e63946", "#f4a261", "#e9c46a", "#2a9d8f", "#457b9d", "#8338ec"]
                        property string activeColor: fullRoot.pick(backend.selectedColor, backend.defaultColor)

                        Repeater {
                            model: parent.colors

                            Rectangle {
                                width: theme.gridUnit * 1.5
                                height: theme.gridUnit * 1.5
                                radius: 4
                                color: modelData
                                border.width: parent.activeColor === modelData ? 2 : 1
                                border.color: parent.activeColor === modelData ? theme.highlightColor : "gray"

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: { fullRoot.setColor(modelData) }
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
                        value: fullRoot.pick(backend.selectedStrokeWidth, backend.defaultStrokeWidth)
                        onMoved: { fullRoot.setStrokeWidth(value) }
                    }

                    Controls.Label {
                        text: Math.round(fullRoot.pick(backend.selectedStrokeWidth, backend.defaultStrokeWidth)) + "px"
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
                        value: fullRoot.pick(backend.selectedOpacity, backend.defaultOpacity)
                        onMoved: { fullRoot.setOpacity(value) }
                    }

                    Controls.Label {
                        text: Math.round(fullRoot.pick(backend.selectedOpacity, backend.defaultOpacity) * 100) + "%"
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

                            property var colors: ["#e63946", "#f4a261", "#e9c46a", "#2a9d8f", "#457b9d", "#8338ec"]
                            property string activeFill: fullRoot.pick(backend.selectedFillColor, backend.defaultFillColor)

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
                                    onClicked: { fullRoot.setFillColor("transparent") }
                                }
                            }

                            Repeater {
                                model: parent.colors

                                Rectangle {
                                    width: theme.gridUnit * 1.5
                                    height: theme.gridUnit * 1.5
                                    radius: 4
                                    color: modelData
                                    border.width: parent.activeFill === modelData ? 2 : 1
                                    border.color: parent.activeFill === modelData ? theme.highlightColor : "gray"

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: { fullRoot.setFillColor(modelData) }
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
                            value: fullRoot.pick(backend.selectedFillOpacity, backend.defaultFillOpacity)
                            onMoved: { fullRoot.setFillOpacity(value) }
                        }

                        Controls.Label {
                            text: Math.round(fullRoot.pick(backend.selectedFillOpacity, backend.defaultFillOpacity) * 100) + "%"
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
                        value: fullRoot.pick(backend.selectedGlow, backend.defaultGlow)
                        onMoved: { fullRoot.setGlow(value) }
                    }

                    Controls.Label {
                        text: Math.round(fullRoot.pick(backend.selectedGlow, backend.defaultGlow)) + "px"
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
                        value: fullRoot.pick(backend.selectedFreehandSmoothing, backend.defaultFreehandSmoothing)
                        onMoved: { fullRoot.setFreehandSmoothing(value) }
                    }

                    Controls.Label {
                        text: {
                            let v = Math.round(fullRoot.pick(backend.selectedFreehandSmoothing, backend.defaultFreehandSmoothing))
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
                        currentIndex: Math.min(2, Math.max(0, Math.round(fullRoot.pick(backend.selectedRoughness, backend.defaultRoughness))))
                        onActivated: { fullRoot.setRoughness(index) }
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
                        value: fullRoot.pick(backend.selectedBorderRadius, backend.defaultBorderRadius)
                        onMoved: { fullRoot.setBorderRadius(value) }
                    }

                    Controls.Label {
                        text: Math.round(fullRoot.pick(backend.selectedBorderRadius, backend.defaultBorderRadius)) + "px"
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
                                let f = fullRoot.pick(backend.selectedFontFamily, backend.defaultFontFamily)
                                let idx = model.indexOf(f)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: { fullRoot.setFontFamily(currentValue) }
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
                            value: fullRoot.pick(backend.selectedFontSize, backend.defaultFontSize)
                            onMoved: { fullRoot.setFontSize(value) }
                        }

                        Controls.Label {
                            text: Math.round(fullRoot.pick(backend.selectedFontSize, backend.defaultFontSize)) + "px"
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
                        model: backend.shapesMetadata

                        delegate: Controls.ItemDelegate {
                            width: shapesListView.width
                            height: theme.gridUnit * 2

                            background: Rectangle {
                                color: (modelData && modelData.selected)
                                       ? theme.highlightColor
                                       : (hovered ? theme.hoverColor : "transparent")
                                opacity: (modelData && modelData.selected) ? 0.3 : 1.0
                                radius: 4
                            }

                            contentItem: RowLayout {
                                spacing: theme.smallSpacing

                                Image {
                                    source: {
                                        let t = modelData && modelData.type ? modelData.type.toLowerCase() : "";
                                        switch(t) {
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
                                    text: (modelData && modelData.name) ? modelData.name : ("Shape " + (index + 1))
                                    font.bold: modelData ? !!modelData.selected : false
                                    Layout.fillWidth: true
                                }

                                Controls.ToolButton {
                                    icon.source: (modelData && modelData.locked) ? "qrc:/icons/object-locked.svg" : "qrc:/icons/object-unlocked.svg"
                                    onClicked: {
                                        backend.setShapeLocked(index, !(modelData && modelData.locked))
                                    }
                                }

                                Controls.ToolButton {
                                    icon.source: "qrc:/icons/edit-delete.svg"
                                    onClicked: {
                                        backend.deleteShape(index)
                                    }
                                }
                            }

                            onClicked: {
                                backend.selectShape(index)
                            }
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
