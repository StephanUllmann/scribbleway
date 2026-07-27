import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Dialogs

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

    // Dynamic backend provider for both Plasma applet (root.backend) and Standalone mode (controller)
    readonly property var backend: (typeof root !== "undefined" && root && root.backend) ? root.backend : (typeof controller !== "undefined" ? controller : null)

    // Track the currently selected tool name for draw mode
    property string activeDrawTool: "freehand"
    property bool isFillableActive: {
        if (backend && backend.hasSelection) {
            let t = backend.selectedType.toLowerCase();
            return t === "rectangle" || t === "ellipse";
        }
        return activeDrawTool === "rectangle" || activeDrawTool === "ellipse";
    }

    property bool isTextActive: {
        if (backend && backend.hasSelection) {
            return backend.selectedType.toLowerCase() === "text";
        }
        return activeDrawTool === "text";
    }

    ColorDialog {
        id: colorDialog
        title: "Choose Custom Color"
        onAccepted: {
            if (backend) backend.setColor(colorDialog.selectedColor.toString())
        }
    }

    ColorDialog {
        id: fillColorDialog
        title: "Choose Fill Color"
        onAccepted: {
            if (backend) backend.setFillColor(fillColorDialog.selectedColor.toString())
        }
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

                Controls.Label {
                    text: (backend && backend.overlayConnected) ? "Running" : "Stopped"
                    font.bold: true
                }
            }

            // Not Running View
            ColumnLayout {
                Layout.fillWidth: true
                visible: backend && !backend.overlayConnected
                spacing: theme.largeSpacing

                Controls.Label {
                    text: "Scribbleway daemon is not running."
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                Controls.Button {
                    text: "Start Scribbleway"
                    icon.source: "qrc:/icons/run-build.svg"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: { if (backend) backend.startOverlay() }
                }
            }

            // Running View
            ColumnLayout {
                Layout.fillWidth: true
                visible: !backend || backend.overlayConnected
                spacing: theme.smallSpacing

                // Mode Toggle Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.smallSpacing

                    Controls.Button {
                        icon.source: "qrc:/icons/draw-freehand.svg"
                        text: "Draw"
                        checkable: true
                        checked: backend ? backend.activeTool !== "select" : true
                        Layout.fillWidth: true
                        onClicked: {
                            if (backend && backend.activeTool === "select") {
                                backend.setActiveTool(fullRoot.activeDrawTool)
                            }
                        }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/edit-select.svg"
                        text: "Select"
                        checkable: true
                        checked: backend ? backend.activeTool === "select" : false
                        Layout.fillWidth: true
                        onClicked: {
                            if (backend) backend.setActiveTool("select")
                        }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/edit-undo.svg"
                        text: "Undo"
                        Layout.fillWidth: true
                        onClicked: { if (backend) backend.undo() }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/edit-redo.svg"
                        text: "Redo"
                        Layout.fillWidth: true
                        onClicked: { if (backend) backend.redo() }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/edit-clear.svg"
                        text: "Clear All"
                        Layout.fillWidth: true
                        onClicked: { if (backend) backend.clearAll() }
                    }
                }

                // Section Header
                Controls.Label {
                    text: (backend && backend.hasSelection) ? "Edit Selection (" + backend.selectedType + ")" : "Default Properties & Tool"
                    font.bold: true
                    font.pixelSize: 11
                    color: theme.highlightColor
                }

                // Selection Management Row
                RowLayout {
                    Layout.fillWidth: true
                    visible: backend && backend.hasSelection
                    spacing: theme.smallSpacing

                    Controls.Button {
                        icon.source: "qrc:/icons/edit-delete.svg"
                        text: "Delete"
                        Layout.fillWidth: true
                        onClicked: { if (backend) backend.deleteSelected() }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/go-up.svg"
                        text: "Raise"
                        onClicked: { if (backend) backend.raiseSelected() }
                    }

                    Controls.Button {
                        icon.source: "qrc:/icons/go-down.svg"
                        text: "Lower"
                        onClicked: { if (backend) backend.lowerSelected() }
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
                        Layout.fillWidth: true
                        model: ["Freehand", "Arrow", "Rectangle", "Ellipse", "Line", "Text"]
                        currentIndex: {
                            if (!backend) return 0;
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
                            if (backend) backend.setActiveTool(selected)
                        }
                    }

                    Controls.Label {
                        text: "Target Screen:"
                    }

                    Controls.ComboBox {
                        Layout.fillWidth: true
                        model: (backend && backend.screenNames) ? backend.screenNames : []
                        currentIndex: {
                            if (!backend || !backend.screenNames) return 0;
                            let idx = backend.screenNames.indexOf(backend.targetScreen)
                            return idx >= 0 ? idx : 0
                        }
                        onActivated: {
                            if (backend) backend.setTargetScreen(currentValue)
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
                        property string activeColor: (backend && backend.hasSelection) ? backend.selectedColor : (backend ? backend.defaultColor : "#e63946")

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
                                    onClicked: { if (backend) backend.setColor(modelData) }
                                }
                            }
                        }

                        Controls.Button {
                            icon.source: "qrc:/icons/color-picker.svg"
                            text: "Custom"
                            onClicked: {
                                colorDialog.selectedColor = parent.activeColor
                                colorDialog.open()
                            }
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
                        value: (backend && backend.hasSelection) ? backend.selectedStrokeWidth : (backend ? backend.defaultStrokeWidth : 2)
                        onMoved: { if (backend) backend.setStrokeWidth(value) }
                    }

                    Controls.Label {
                        text: Math.round((backend && backend.hasSelection) ? backend.selectedStrokeWidth : (backend ? backend.defaultStrokeWidth : 2)) + "px"
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
                        value: (backend && backend.hasSelection) ? backend.selectedOpacity : (backend ? backend.defaultOpacity : 1.0)
                        onMoved: { if (backend) backend.setOpacity(value) }
                    }

                    Controls.Label {
                        text: Math.round(((backend && backend.hasSelection) ? backend.selectedOpacity : (backend ? backend.defaultOpacity : 1.0)) * 100) + "%"
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
                            property string activeFill: backend ? backend.selectedFillColor : "transparent"

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
                                    onClicked: { if (backend) backend.setFillColor("transparent") }
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
                                        onClicked: { if (backend) backend.setFillColor(modelData) }
                                    }
                                }
                            }

                            Controls.Button {
                                icon.source: "qrc:/icons/color-picker.svg"
                                text: "Custom"
                                onClicked: {
                                    fillColorDialog.selectedColor = parent.activeFill !== "transparent" ? parent.activeFill : "#e63946"
                                    fillColorDialog.open()
                                }
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
                            value: backend ? backend.selectedFillOpacity : 0.12
                            onMoved: { if (backend) backend.setFillOpacity(value) }
                        }

                        Controls.Label {
                            text: Math.round((backend ? backend.selectedFillOpacity : 0.12) * 100) + "%"
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
                        value: backend ? backend.selectedGlow : 10
                        onMoved: { if (backend) backend.setGlow(value) }
                    }

                    Controls.Label {
                        text: Math.round(backend ? backend.selectedGlow : 10) + "px"
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
                        value: backend ? backend.selectedFreehandSmoothing : 2
                        onMoved: { if (backend) backend.setFreehandSmoothing(value) }
                    }

                    Controls.Label {
                        text: {
                            let v = Math.round(backend ? backend.selectedFreehandSmoothing : 2)
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
                        Layout.fillWidth: true
                        model: ["Neat (0)", "Artist (1)", "Cartoon (2)"]
                        currentIndex: Math.min(2, Math.max(0, Math.round(backend ? backend.selectedRoughness : 1)))
                        onActivated: { if (backend) backend.setRoughness(index) }
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
                        value: backend ? backend.selectedBorderRadius : 8
                        onMoved: { if (backend) backend.setBorderRadius(value) }
                    }

                    Controls.Label {
                        text: Math.round(backend ? backend.selectedBorderRadius : 8) + "px"
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
                            Layout.fillWidth: true
                            model: ["sans-serif", "serif", "monospace", "Comic Sans MS"]
                            currentIndex: {
                                if (!backend) return 0;
                                let f = backend.selectedFontFamily
                                let idx = model.indexOf(f)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: { if (backend) backend.setFontFamily(currentValue) }
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
                            value: backend ? backend.selectedFontSize : 20
                            onMoved: { if (backend) backend.setFontSize(value) }
                        }

                        Controls.Label {
                            text: Math.round(backend ? backend.selectedFontSize : 20) + "px"
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
                        model: backend ? backend.shapesMetadata : []

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
                                            case "rectangle": return "qrc:/icons/edit-select.svg";
                                            case "ellipse": return "qrc:/icons/edit-select.svg";
                                            case "arrow": return "qrc:/icons/arrow-right.svg";
                                            case "line": return "qrc:/icons/arrow-right.svg";
                                            case "text": return "qrc:/icons/draw-freehand.svg";
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
                                        if (backend) backend.setShapeLocked(index, !(modelData && modelData.locked))
                                    }
                                }

                                Controls.ToolButton {
                                    icon.source: "qrc:/icons/edit-delete.svg"
                                    onClicked: {
                                        if (backend) backend.deleteShape(index)
                                    }
                                }
                            }

                            onClicked: {
                                if (backend) backend.selectShape(index)
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
