import QtQuick
import QtQuick.Layouts
// Basic, not the platform style: org.kde.desktop paints whole controls through QStyle, so
// their parts cannot be restyled, and the popup keeps one look on every platform.
import QtQuick.Controls.Basic as Controls
import "popup"

Item {
    id: fullRoot

    implicitWidth: 432
    implicitHeight: 550
    Layout.minimumWidth: 400
    Layout.minimumHeight: 520
    Layout.preferredWidth: 432
    Layout.preferredHeight: 550

    Theme { id: theme }

    readonly property var backend: controller

    // The selected shape's value if there is one, else the style for new shapes.
    // `selection` stays current because updateProperties() is the only way to write either.
    function get(key) {
        return backend.selection[key]
    }

    // updateProperties writes the key into `style` and, if there is a selection,
    // the selected shapes too; it also keeps a filled shape's fill in its stroke color.
    function set(key, value) {
        let props = {}
        props[key] = value
        backend.updateProperties(props)
    }

    readonly property string activeType: (backend.hasSelection ? backend.selection.type : backend.activeTool).toLowerCase()
    readonly property bool isFillableActive: activeType === "rectangle" || activeType === "ellipse"
    readonly property bool isTextActive: activeType === "text"

    // Handled by whoever hosts this menu (TrayPopup), which owns the picker panel — a
    // QtQuick.Dialogs ColorDialog is an xdg-desktop-portal dialog and opens fullscreen
    // on the primary output, which is never where the popup is.
    signal customColorRequested(string current, bool isFill)

    function applyCustomColor(c, isFill) {
        set(isFill ? "fillColor" : "color", c.toString())
    }

    readonly property var tools: [
        { id: "freehand", name: "Freehand" },
        { id: "arrow", name: "Arrow" },
        { id: "rectangle", name: "Rectangle" },
        { id: "ellipse", name: "Ellipse" },
        { id: "line", name: "Line" },
        { id: "text", name: "Text" },
    ]
    readonly property var roughnessNames: ["Neat", "Artist", "Cartoon"]
    readonly property var fontFamilies: ["sans-serif", "serif", "monospace", "Comic Sans MS"]

    function iconFor(type) {
        switch (type.toLowerCase()) {
            case "rectangle": return "qrc:/icons/draw-rectangle.svg"
            case "ellipse": return "qrc:/icons/draw-ellipse.svg"
            case "arrow": return "qrc:/icons/draw-arrow.svg"
            case "line": return "qrc:/icons/draw-line.svg"
            case "text": return "qrc:/icons/draw-text.svg"
            default: return "qrc:/icons/draw-freehand.svg"
        }
    }

    function capitalized(s) {
        return s.charAt(0).toUpperCase() + s.slice(1)
    }

    // The key the user has bound to a hotkey id, so the popup teaches the real binding.
    function hotkey(id) {
        return backend.localShortcutSequences[id] || ""
    }

    component SectionHeading: Label {
        Layout.topMargin: 10
        Layout.bottomMargin: 2
        color: theme.ink
        font.pixelSize: theme.body
        font.weight: Font.DemiBold
    }

    component Page: Controls.ScrollView {
        contentWidth: availableWidth
    }

    // --- Layout ---

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Mode, history
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: theme.pad
            Layout.bottomMargin: theme.gap
            spacing: 6

            Rectangle {
                implicitWidth: modeRow.implicitWidth + 4
                implicitHeight: 30
                radius: theme.radius
                color: theme.surface
                RowLayout {
                    id: modeRow
                    anchors.fill: parent
                    anchors.margins: 2
                    spacing: 2
                    SegmentButton {
                        text: "Draw"
                        checked: backend.currentMode === "draw"
                        onClicked: {
                            if (backend.currentMode !== "draw") {
                                backend.setActiveTool("freehand")
                            }
                        }
                    }
                    SegmentButton {
                        objectName: "selectModeButton"
                        text: "Select"
                        checked: backend.currentMode === "select"
                        onClicked: backend.enterSelectMode()
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Shown when the app runs with --dev. `devMode` is a context property
            // set by main.cpp; tests that load this file alone leave it undefined.
            IconButton {
                objectName: "devModeIcon"
                visible: typeof devMode !== "undefined" && devMode
                flat: true
                icon.source: "qrc:/icons/dev-mode.svg"
                icon.color: "#f0b44c"
                tip: "Dev mode: logging to ~/.local/state/scribbleway/logs"
            }
            IconButton {
                icon.source: "qrc:/icons/edit-undo.svg"
                tip: "Undo  " + fullRoot.hotkey("action_undo")
                onClicked: backend.undo()
            }
            IconButton {
                icon.source: "qrc:/icons/edit-redo.svg"
                tip: "Redo  " + fullRoot.hotkey("action_redo")
                onClicked: backend.redo()
            }
        }

        // Tools, each marked with its hotkey
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: theme.pad
            Layout.rightMargin: theme.pad
            spacing: 4

            Repeater {
                model: fullRoot.tools
                Controls.ToolButton {
                    id: toolCell
                    required property var modelData
                    readonly property string key: fullRoot.hotkey("tool_" + modelData.id)
                    Layout.fillWidth: true
                    implicitHeight: 38
                    padding: 0
                    display: Controls.AbstractButton.IconOnly
                    checked: backend.activeTool.toLowerCase() === modelData.id
                    icon.source: fullRoot.iconFor(modelData.id)
                    icon.width: 20
                    icon.height: 20
                    icon.color: checked ? theme.accentInk : theme.ink
                    onClicked: backend.setActiveTool(modelData.id)
                    background: Rectangle {
                        radius: theme.radius
                        color: toolCell.checked ? theme.accent
                             : toolCell.down ? theme.pressed
                             : toolCell.hovered ? theme.raised : theme.surface
                        border.width: toolCell.visualFocus ? 2 : 0
                        border.color: toolCell.checked ? theme.ink : theme.accent
                    }
                    Label {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.rightMargin: 5
                        anchors.topMargin: 3
                        text: toolCell.key
                        font.family: theme.mono
                        font.pixelSize: theme.small
                        font.weight: Font.DemiBold
                        color: toolCell.checked ? theme.accentInk : theme.mute
                    }
                    Accessible.name: modelData.name
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: modelData.name + (key !== "" ? "  " + key : "")
                    Controls.ToolTip.delay: 600
                }
            }
        }

        Controls.TabBar {
            id: tabs
            Layout.fillWidth: true
            Layout.topMargin: 6
            leftPadding: theme.pad + 2
            rightPadding: theme.pad
            spacing: 20
            background: Item {
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: theme.line
                }
            }

            Repeater {
                model: ["Style", "Shapes", "More"]
                Controls.TabButton {
                    id: tab
                    required property int index
                    required property string modelData
                    text: modelData
                    width: implicitWidth
                    implicitHeight: 36
                    leftPadding: 0
                    rightPadding: 0
                    contentItem: Row {
                        spacing: 6
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: tab.text
                            color: tab.checked || tab.hovered ? theme.ink : theme.mute
                            font.pixelSize: theme.body
                            font.weight: tab.checked ? Font.DemiBold : Font.Medium
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: tab.index === 1
                            text: shapesListView.count
                            color: theme.mute
                            font.pixelSize: theme.body
                        }
                    }
                    background: Item {
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 2
                            color: tab.checked ? theme.accent : "transparent"
                        }
                        Rectangle {
                            visible: tab.visualFocus
                            anchors.fill: parent
                            anchors.margins: 4
                            radius: 4
                            color: "transparent"
                            border.color: theme.accent
                        }
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            // Style: the selected Shape, or the style for the next one
            Page {
                ColumnLayout {
                    width: parent.width
                    spacing: 2

                    SpecimenWell {
                        objectName: "specimenWell"
                        Layout.fillWidth: true
                        Layout.leftMargin: theme.pad
                        Layout.rightMargin: theme.pad
                        Layout.topMargin: theme.pad
                        Layout.bottomMargin: 6
                        Layout.preferredHeight: 128
                        type: fullRoot.activeType !== "" ? fullRoot.activeType : "rectangle"
                        roughnessNames: fullRoot.roughnessNames
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: theme.pad + 2
                        Layout.rightMargin: theme.pad + 2
                        spacing: 0

                        FieldRow {
                            id: strokeRow
                            label: fullRoot.isTextActive ? "Colour" : "Stroke"
                            readonly property string current: String(fullRoot.get("color")).toLowerCase()
                            Repeater {
                                model: backend.presetColors
                                Swatch {
                                    required property int index
                                    required property string modelData
                                    swatchColor: modelData
                                    selected: strokeRow.current === modelData.toLowerCase()
                                    hotkey: fullRoot.hotkey("color_" + (index + 1))
                                    Accessible.name: "Stroke colour " + (index + 1)
                                    onClicked: fullRoot.set("color", modelData)
                                }
                            }
                            CustomSwatch {
                                current: strokeRow.current
                                onClicked: fullRoot.customColorRequested(current, false)
                            }
                        }

                        FieldRow {
                            visible: !fullRoot.isTextActive
                            label: "Width"
                            value: Math.round(fullRoot.get("strokeWidth")) + " px"
                            EditSlider {
                                from: 1
                                to: 15
                                stepSize: 1
                                value: fullRoot.get("strokeWidth")
                                onMoved: fullRoot.set("strokeWidth", value)
                            }
                        }

                        FieldRow {
                            label: "Opacity"
                            value: Math.round(fullRoot.get("opacity") * 100) + "%"
                            EditSlider {
                                from: 0.1
                                to: 1.0
                                stepSize: 0.05
                                value: fullRoot.get("opacity")
                                onMoved: fullRoot.set("opacity", value)
                            }
                        }

                        FieldRow {
                            id: fillRow
                            visible: fullRoot.isFillableActive
                            label: "Fill"
                            readonly property string current: String(fullRoot.get("fillColor")).toLowerCase()
                            Swatch {
                                none: true
                                selected: fillRow.current === "transparent"
                                Accessible.name: "No fill"
                                onClicked: fullRoot.set("fillColor", "transparent")
                            }
                            Repeater {
                                model: backend.presetColors
                                Swatch {
                                    required property int index
                                    required property string modelData
                                    swatchColor: modelData
                                    selected: fillRow.current === modelData.toLowerCase()
                                    Accessible.name: "Fill colour " + (index + 1)
                                    onClicked: fullRoot.set("fillColor", modelData)
                                }
                            }
                            CustomSwatch {
                                current: fillRow.current
                                onClicked: fullRoot.customColorRequested(
                                    current !== "transparent" ? current : "#e63946", true)
                            }
                        }

                        FieldRow {
                            visible: fullRoot.isFillableActive
                            label: "Fill opacity"
                            value: Math.round(fullRoot.get("fillOpacity") * 100) + "%"
                            EditSlider {
                                from: 0.0
                                to: 1.0
                                stepSize: 0.05
                                value: fullRoot.get("fillOpacity")
                                onMoved: fullRoot.set("fillOpacity", value)
                            }
                        }

                        FieldRow {
                            visible: !fullRoot.isTextActive
                            label: "Roughness"
                            ChoiceBar {
                                options: fullRoot.roughnessNames
                                current: Math.min(2, Math.max(0, Math.round(fullRoot.get("roughness"))))
                                onPicked: (index) => fullRoot.set("roughness", index)
                            }
                        }

                        FieldRow {
                            visible: fullRoot.isTextActive
                            label: "Font"
                            ChoiceBar {
                                options: ["Sans", "Serif", "Mono", "Comic"]
                                current: Math.max(0, fullRoot.fontFamilies.indexOf(fullRoot.get("fontFamily")))
                                onPicked: (index) => fullRoot.set("fontFamily", fullRoot.fontFamilies[index])
                            }
                        }

                        FieldRow {
                            visible: fullRoot.isTextActive
                            label: "Size"
                            value: Math.round(fullRoot.get("fontSize")) + " px"
                            EditSlider {
                                from: 10
                                to: 72
                                stepSize: 1
                                value: fullRoot.get("fontSize")
                                onMoved: fullRoot.set("fontSize", value)
                            }
                        }
                    }
                }
            }

            // Shapes: every drawn Shape, newest last
            ColumnLayout {
                spacing: 0

                ListView {
                    id: shapesListView
                    objectName: "shapesListView"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: theme.gap
                    clip: true
                    spacing: 2
                    // The shapes model itself, not a parallel metadata copy of it:
                    // it already carries type/selected/locked and signals its own changes.
                    model: backend.shapesModel
                    Controls.ScrollBar.vertical: Controls.ScrollBar {
                        visible: size < 1.0
                        contentItem: Rectangle {
                            implicitWidth: 4
                            radius: 2
                            color: theme.raised
                        }
                    }

                    delegate: Controls.ItemDelegate {
                        id: shapeRow
                        required property int index
                        required property string type
                        // var, not bool: these keys are optional on a shape map, and
                        // an absent one arrives as undefined.
                        required property var selected
                        required property var locked

                        width: ListView.view.width
                        height: 36
                        leftPadding: 10
                        rightPadding: 76
                        text: fullRoot.capitalized(type) + " " + (index + 1)
                        icon.source: fullRoot.iconFor(type)
                        icon.width: 16
                        icon.height: 16
                        icon.color: theme.ink
                        font.family: theme.sans
                        font.pixelSize: theme.body
                        font.weight: !!selected ? Font.DemiBold : Font.Normal
                        palette.text: theme.ink
                        palette.highlightedText: theme.ink

                        background: Rectangle {
                            radius: 6
                            color: !!shapeRow.selected ? theme.selectedRow
                                 : shapeRow.hovered ? theme.surface : "transparent"
                            border.width: shapeRow.visualFocus ? 1 : 0
                            border.color: theme.accent
                        }

                        Row {
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            IconButton {
                                flat: true
                                implicitWidth: 28
                                implicitHeight: 28
                                icon.width: 16
                                icon.height: 16
                                icon.source: !!shapeRow.locked ? "qrc:/icons/object-locked.svg"
                                                               : "qrc:/icons/object-unlocked.svg"
                                icon.color: !!shapeRow.locked ? theme.accent : theme.mute
                                tip: !!shapeRow.locked ? "Unlock" : "Lock"
                                onClicked: backend.setShapeLocked(shapeRow.index, !shapeRow.locked)
                            }
                            IconButton {
                                flat: true
                                implicitWidth: 28
                                implicitHeight: 28
                                icon.width: 16
                                icon.height: 16
                                icon.source: "qrc:/icons/edit-delete.svg"
                                icon.color: hovered ? theme.danger : theme.mute
                                tip: "Delete"
                                onClicked: backend.deleteShape(shapeRow.index)
                            }
                        }

                        onClicked: backend.selectShape(shapeRow.index)
                    }

                    Column {
                        anchors.centerIn: parent
                        width: parent.width - 48
                        spacing: 4
                        visible: shapesListView.count === 0
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: "Nothing drawn yet"
                            color: theme.ink
                            font.pixelSize: theme.body
                            font.weight: Font.DemiBold
                        }
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: "Pick a tool above and draw on the screen. Every shape you draw is listed here."
                            color: theme.mute
                            font.pixelSize: theme.body
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: theme.line
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: theme.pad
                    Label {
                        Layout.fillWidth: true
                        text: shapesListView.count === 1 ? "1 shape" : shapesListView.count + " shapes"
                        color: theme.mute
                        font.pixelSize: theme.body
                    }
                    TextButton {
                        text: "Clear all"
                        tone: theme.danger
                        enabled: shapesListView.count > 0
                        onClicked: backend.clear()
                    }
                }
            }

            // More: settings touched rarely
            Page {
                ColumnLayout {
                    width: parent.width
                    spacing: 0

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: theme.pad + 2
                        Layout.topMargin: 2
                        spacing: 0

                        SectionHeading { text: "Effects" }

                        FieldRow {
                            label: "Glow"
                            value: Math.round(fullRoot.get("glow")) + " px"
                            EditSlider {
                                from: 0
                                to: 30
                                stepSize: 1
                                value: fullRoot.get("glow")
                                onMoved: fullRoot.set("glow", value)
                            }
                        }

                        FieldRow {
                            label: "Corners"
                            value: Math.round(fullRoot.get("borderRadius")) + " px"
                            EditSlider {
                                from: 0
                                to: 50
                                stepSize: 1
                                value: fullRoot.get("borderRadius")
                                onMoved: fullRoot.set("borderRadius", value)
                            }
                        }

                        FieldRow {
                            label: "Smoothing"
                            ChoiceBar {
                                options: ["Off", "Low", "Med", "High"]
                                current: Math.min(3, Math.max(0, Math.round(fullRoot.get("freehandSmoothing"))))
                                onPicked: (index) => fullRoot.set("freehandSmoothing", index)
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            Layout.leftMargin: theme.labelWidth + 10
                            text: "Corners apply to rectangles, smoothing to new freehand strokes."
                            wrapMode: Text.WordWrap
                            color: theme.mute
                            font.pixelSize: 12
                        }

                        SectionHeading { text: "Overlay"; Layout.topMargin: 18 }

                        FieldRow {
                            label: "Screen"
                            Controls.ComboBox {
                                id: screenBox
                                // In-window dropdown: a Popup.Window one is a separate Wayland
                                // surface, and the tray popup dismisses itself on focus loss.
                                Component.onCompleted: popup.popupType = Controls.Popup.Item
                                Layout.fillWidth: true
                                implicitHeight: 30
                                font.family: theme.sans
                                font.pixelSize: theme.body
                                model: overlayWindow.screenLabels
                                currentIndex: {
                                    let idx = overlayWindow.screenNames.indexOf(overlayWindow.targetScreen)
                                    return idx >= 0 ? idx : 0
                                }
                                onActivated: (index) => overlayWindow.setTargetScreen(overlayWindow.screenNames[index])
                                background: Rectangle {
                                    radius: theme.radius
                                    color: screenBox.down ? theme.pressed : screenBox.hovered ? theme.raised : theme.surface
                                    border.width: screenBox.visualFocus ? 1 : 0
                                    border.color: theme.accent
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
