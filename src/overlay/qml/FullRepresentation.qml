import QtQuick
import QtQuick.Layouts
// Basic, not the platform style: org.kde.desktop paints whole controls through QStyle, so
// their parts cannot be restyled, and the popup keeps one look on every platform.
import QtQuick.Controls.Basic as Controls
import "shapes/RoughPathGenerator.js" as RoughPathGenerator

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

    // --- The specimen: the selected Shape (or the next one) drawn by the canvas delegates ---

    readonly property string specimenType: activeType !== "" ? activeType : "rectangle"
    readonly property var styleKeys: ["color", "strokeWidth", "opacity", "fontFamily", "fontSize",
                                      "borderRadius", "roughness", "glow", "fillColor", "fillOpacity"]

    function specimenShape(type, w, h) {
        const s = backend.selection
        // Without a selection, exactly the keys the next Shape of this type will carry.
        const style = backend.hasSelection ? s : backend.styleForNewShape(type)
        let shape = { type: type, seed: s.seed !== undefined ? s.seed : 123456 }
        for (const k of styleKeys) {
            if (style[k] !== undefined) shape[k] = style[k]
        }
        const bw = Math.min(180, w - 48), bh = 62
        const x = (w - bw) / 2, y = 38
        if (type === "line" || type === "arrow") {
            Object.assign(shape, { fromX: x, fromY: y + bh, toX: x + bw, toY: y })
        } else if (type === "freehand") {
            // A jittery scribble, so the Smoothing setting visibly changes it.
            let pts = []
            for (let i = 0; i <= 48; ++i) {
                const t = i / 48
                const jitter = ((i * 37) % 7 - 3) * 1.4
                pts.push(Qt.point(x + t * bw, y + bh / 2 + Math.sin(t * Math.PI * 2.4) * bh * 0.38 + jitter))
            }
            shape.points = RoughPathGenerator.smoothFreehandPoints(pts, s.freehandSmoothing)
        } else if (type === "text") {
            Object.assign(shape, { x: 16, y: 30, width: w - 32, height: bh, text: "Aa Bb Cc 123" })
        } else {
            Object.assign(shape, { x: x, y: y, width: bw, height: bh })
        }
        return shape
    }

    readonly property var specimen: specimenShape(specimenType, specimenWell.width, specimenWell.height)
    onSpecimenChanged: backend.setSpecimenShape(specimen)

    readonly property string specimenReadout: {
        const s = specimen
        const pct = v => Math.round(v * 100) + "%"
        if (specimenType === "text") {
            return s.fontFamily + " · " + Math.round(s.fontSize) + " px · " + pct(s.opacity)
        }
        let parts = ["W " + Math.round(s.strokeWidth) + " px", pct(s.opacity)]
        if ((specimenType === "rectangle" || specimenType === "ellipse") && s.fillColor !== "transparent") {
            parts.push("fill " + pct(s.fillOpacity))
        }
        parts.push(roughnessNames[Math.min(2, Math.max(0, Math.round(s.roughness)))])
        return parts.join(" · ")
    }

    readonly property string specimenName: backend.hasSelection
        ? capitalized(backend.selection.type) + " " + (backend.selection.selectedIndex + 1)
          + (backend.hasMultiSelection ? " and others" : "")
        : "New " + specimenType

    // --- Building blocks, all drawn from Theme ---

    // All popup text. The family is pinned so the popup keeps its own look whatever
    // font the desktop uses for its own UI.
    component Label: Text {
        color: theme.ink
        font.family: theme.sans
        font.pixelSize: theme.body
    }

    component IconButton: Controls.ToolButton {
        id: iconButton
        property string tip: ""
        display: Controls.AbstractButton.IconOnly
        icon.width: 18
        icon.height: 18
        icon.color: enabled ? theme.ink : theme.mute
        implicitWidth: 30
        implicitHeight: 30
        padding: 0
        background: Rectangle {
            radius: theme.radius
            color: iconButton.down ? theme.pressed
                 : iconButton.hovered ? theme.raised
                 : iconButton.flat ? "transparent" : theme.surface
            border.width: iconButton.visualFocus ? 1 : 0
            border.color: theme.accent
        }
        Accessible.name: tip
        Controls.ToolTip.visible: hovered && tip !== ""
        Controls.ToolTip.text: tip
        Controls.ToolTip.delay: 600
    }

    component TextButton: Controls.Button {
        id: textButton
        property color tone: theme.ink
        implicitHeight: 26
        leftPadding: 10
        rightPadding: 10
        topPadding: 0
        bottomPadding: 0
        font.family: theme.sans
        font.pixelSize: 12
        contentItem: Text {
            text: textButton.text
            font: textButton.font
            color: textButton.enabled ? textButton.tone : theme.mute
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 5
            color: textButton.down ? theme.pressed : textButton.hovered ? theme.raised : theme.surface
            border.width: textButton.visualFocus ? 1 : 0
            border.color: theme.accent
        }
    }

    component SegmentButton: Controls.AbstractButton {
        id: segment
        implicitHeight: 26
        implicitWidth: segmentLabel.implicitWidth + 20
        Layout.fillWidth: true
        contentItem: Label {
            id: segmentLabel
            text: segment.text
            color: segment.checked ? theme.ink : theme.mute
            font.pixelSize: theme.body
            font.weight: segment.checked ? Font.DemiBold : Font.Normal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: theme.radius - 2
            color: segment.checked ? theme.raised : segment.hovered ? "#242424" : "transparent"
            border.width: segment.visualFocus ? 1 : 0
            border.color: theme.accent
        }
    }

    // A segmented control over string options; `current` is the picked index.
    component ChoiceBar: Rectangle {
        id: choiceBar
        property var options: []
        property int current: -1
        signal picked(int index)
        Layout.fillWidth: true
        implicitHeight: 30
        radius: theme.radius
        color: theme.surface
        RowLayout {
            anchors.fill: parent
            anchors.margins: 2
            spacing: 2
            Repeater {
                model: choiceBar.options
                SegmentButton {
                    required property int index
                    required property string modelData
                    text: modelData
                    checked: index === choiceBar.current
                    onClicked: choiceBar.picked(index)
                }
            }
        }
    }

    // A slider drag is one undo step, however many values it passes through.
    component EditSlider: Controls.Slider {
        id: slider
        Layout.fillWidth: true
        onPressedChanged: pressed ? controller.beginEdit() : controller.endEdit()
        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 4
            radius: 2
            color: theme.raised
            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 2
                color: theme.accent
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 16
            height: 16
            radius: 8
            color: slider.pressed || slider.hovered ? "#ffffff" : theme.ink
            border.width: slider.visualFocus ? 2 : 0
            border.color: theme.accent
        }
    }

    component Swatch: Controls.AbstractButton {
        id: swatch
        property color swatchColor: "transparent"
        property bool none: false     // the "no fill" cell
        property bool selected: false
        property string hotkey: ""
        implicitWidth: 26
        implicitHeight: 26
        leftPadding: 3
        bottomPadding: 2
        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                radius: 8
                color: "transparent"
                border.width: 2
                border.color: swatch.selected ? theme.ink
                            : swatch.visualFocus ? theme.accent
                            : swatch.hovered ? theme.mute : "transparent"
            }
            Rectangle {
                anchors.fill: parent
                radius: 5
                clip: true
                color: swatch.none ? theme.surface : swatch.swatchColor
                border.width: swatch.none ? 1 : 0
                border.color: theme.raised
                Rectangle {
                    visible: swatch.none
                    anchors.centerIn: parent
                    width: parent.width * 1.2
                    height: 2
                    rotation: -45
                    color: theme.danger
                }
            }
        }
        contentItem: Text {
            text: swatch.hotkey
            font.family: theme.mono
            font.pixelSize: theme.small
            font.weight: Font.Bold
            color: theme.inkOn(swatch.swatchColor)
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignBottom
        }
    }

    // Opens the picker. Shows the current colour when it is not one of the presets.
    component CustomSwatch: Controls.ToolButton {
        id: customSwatch
        property string current: ""
        readonly property bool isCustom: current !== "" && current !== "transparent"
                                         && backend.presetColors.indexOf(current) < 0
        display: Controls.AbstractButton.IconOnly
        // The current custom colour shows as the colour alone; "+" only offers a new one.
        icon.source: isCustom ? "" : "qrc:/icons/list-add.svg"
        icon.width: 14
        icon.height: 14
        icon.color: isCustom ? theme.inkOn(current) : theme.ink
        implicitWidth: 26
        implicitHeight: 26
        padding: 0
        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                radius: 8
                color: "transparent"
                border.width: 2
                border.color: customSwatch.isCustom ? theme.ink
                            : customSwatch.visualFocus ? theme.accent
                            : customSwatch.hovered ? theme.mute : "transparent"
            }
            Rectangle {
                anchors.fill: parent
                radius: 5
                color: customSwatch.isCustom ? customSwatch.current
                     : customSwatch.down ? theme.pressed : theme.raised
            }
        }
        Accessible.name: "Custom colour"
        Controls.ToolTip.visible: hovered
        Controls.ToolTip.text: "Custom colour"
        Controls.ToolTip.delay: 600
    }

    // One aligned row: fixed label column, the controls, and an optional value column.
    component FieldRow: RowLayout {
        property alias label: fieldLabel.text
        property string value: ""
        default property alias content: slot.data
        Layout.fillWidth: true
        Layout.minimumHeight: 34
        spacing: 10
        Label {
            id: fieldLabel
            Layout.preferredWidth: theme.labelWidth
            color: theme.mute
            font.pixelSize: theme.body
            elide: Text.ElideRight
        }
        RowLayout {
            id: slot
            Layout.fillWidth: true
            spacing: 7
        }
        Label {
            visible: parent.value !== ""
            text: parent.value
            Layout.preferredWidth: theme.valueWidth
            horizontalAlignment: Text.AlignRight
            color: theme.ink
            font.pixelSize: theme.body
            font.features: { "tnum": 1 }    // values stay put while a slider drags
        }
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

                    Rectangle {
                        id: specimenWell
                        objectName: "specimenWell"
                        Layout.fillWidth: true
                        Layout.leftMargin: theme.pad
                        Layout.rightMargin: theme.pad
                        Layout.topMargin: theme.pad
                        Layout.bottomMargin: 6
                        Layout.preferredHeight: 128
                        radius: 10
                        color: theme.well
                        border.color: theme.line
                        clip: true

                        // The same delegates as the canvas, fed one row: what you see here
                        // is how the Shape is drawn, roughness, glow and all.
                        Repeater {
                            model: backend.specimenModel
                            delegate: Loader {
                                anchors.fill: parent
                                enabled: false
                                source: {
                                    switch (model.type) {
                                        case "rectangle": return "shapes/RectangleShape.qml"
                                        case "ellipse": return "shapes/EllipseShape.qml"
                                        case "line": return "shapes/LineShape.qml"
                                        case "arrow": return "shapes/ArrowShape.qml"
                                        case "freehand": return "shapes/FreehandShape.qml"
                                        case "text": return "shapes/TextShape.qml"
                                    }
                                    return ""
                                }
                            }
                        }

                        Label {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.margins: 10
                            text: fullRoot.specimenName
                            color: backend.hasSelection ? theme.ink : theme.mute
                            font.pixelSize: theme.body
                            font.weight: Font.DemiBold
                        }

                        Row {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 6
                            spacing: 4
                            visible: backend.hasSelection
                            TextButton { text: "Raise"; onClicked: backend.raiseSelected() }
                            TextButton { text: "Lower"; onClicked: backend.lowerSelected() }
                            TextButton { text: "Delete"; tone: theme.danger; onClicked: backend.deleteSelected() }
                        }

                        Label {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            anchors.margins: 10
                            text: fullRoot.specimenReadout.toUpperCase()
                            color: theme.mute
                            font.family: theme.mono
                            font.pixelSize: theme.small
                            font.letterSpacing: 0.8
                            font.features: { "tnum": 1 }
                        }
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
