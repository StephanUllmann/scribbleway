import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras

ColumnLayout {
    id: fullRoot
    
    implicitWidth: Kirigami.Units.gridUnit * 24
    implicitHeight: Kirigami.Units.gridUnit * 28
    
    spacing: Kirigami.Units.smallSpacing

    ColorDialog {
        id: colorDialog
        title: "Choose Custom Color"
        onAccepted: {
            root.backend.setColor(colorDialog.selectedColor.toString())
        }
    }

    // Track the currently selected tool name for draw mode
    property string currentToolName: "freehand"
    property string recordingActionId: ""
    property string reassignmentNoticeText: ""
    property bool isRectActive: (root.backend.hasSelection && root.backend.selectedType.toLowerCase() === "rectangle") || (!root.backend.hasSelection && currentToolName === "rectangle")

    Timer {
        id: reassignmentNoticeTimer
        interval: 4000
        onTriggered: reassignmentNoticeText = ""
    }

    Connections {
        target: root.backend
        function onActiveToolChanged() {
            let tool = root.backend.activeTool;
            if (tool && tool !== "") {
                fullRoot.currentToolName = tool;
            }
        }
    }

    // Header / Daemon Status
    RowLayout {
        Layout.fillWidth: true
        
        PlasmaExtras.Heading {
            level: 3
            text: "ScribbleWay"
            Layout.fillWidth: true
        }

        Rectangle {
            width: 10
            height: 10
            radius: 5
            color: root.backend.overlayConnected ? "#2a9d8f" : "#e63946"
        }

        PlasmaComponents.Label {
            text: root.backend.overlayConnected ? "Running" : "Stopped"
            font.bold: true
        }
    }

    // Connect Button when stopped
    ColumnLayout {
        Layout.fillWidth: true
        visible: !root.backend.overlayConnected
        spacing: Kirigami.Units.largeSpacing
        
        PlasmaComponents.Label {
            text: "ScribbleWay daemon is not running. Start it to enable global shortcuts and screen annotations."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        PlasmaComponents.Button {
            text: "Start ScribbleWay"
            icon.name: "run-build"
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                root.backend.startDaemon()
            }
        }
        
        Item { Layout.fillHeight: true }
    }

    // Controls when daemon is running
    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.backend.overlayConnected
        spacing: Kirigami.Units.smallSpacing

        // Mode Toggle Row
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.Button {
                icon.name: "draw-freehand"
                text: "Draw"
                checkable: true
                checked: root.backend.currentMode === "draw"
                Layout.fillWidth: true
                onClicked: {
                    if (root.backend.currentMode === "draw") {
                        root.backend.enterPassthroughMode()
                    } else {
                        root.backend.setTool(fullRoot.currentToolName)
                    }
                }
            }

            PlasmaComponents.Button {
                icon.name: "edit-select"
                text: "Select"
                checkable: true
                checked: root.backend.currentMode === "select"
                Layout.fillWidth: true
                onClicked: {
                    if (root.backend.currentMode === "select") {
                        root.backend.enterPassthroughMode()
                    } else {
                        root.backend.enterSelectMode()
                    }
                }
            }
        }

        // Row 1: Global Actions
        RowLayout {
            Layout.fillWidth: true
            
            PlasmaComponents.Button {
                icon.name: "edit-undo"
                text: "Undo"
                Layout.fillWidth: true
                onClicked: root.backend.undo()
            }

            PlasmaComponents.Button {
                icon.name: "edit-redo"
                text: "Redo"
                Layout.fillWidth: true
                onClicked: root.backend.redo()
            }

            PlasmaComponents.Button {
                icon.name: "edit-clear"
                text: "Clear All"
                Layout.fillWidth: true
                onClicked: root.backend.clear()
            }
        }

        // Section: Properties & Tool
        PlasmaComponents.Label {
            text: root.backend.hasSelection ? "Edit Selection (" + root.backend.selectedType + ")" : "Default Properties & Tool"
            font.bold: true
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            color: Kirigami.Theme.highlightColor
        }

        // Selection management row (only visible when shape is selected)
        RowLayout {
            Layout.fillWidth: true
            visible: root.backend.hasSelection

            PlasmaComponents.Button {
                icon.name: "edit-delete"
                text: "Delete"
                Layout.fillWidth: true
                onClicked: root.backend.deleteSelected()
            }

            PlasmaComponents.Button {
                icon.name: "go-up"
                text: "Raise"
                onClicked: root.backend.raiseSelected()
            }

            PlasmaComponents.Button {
                icon.name: "go-down"
                text: "Lower"
                onClicked: root.backend.lowerSelected()
            }
        }

        // Row 2: Tool selection (always available)
        RowLayout {
            Layout.fillWidth: true
            
            PlasmaComponents.Label {
                text: "Active Tool:"
            }
            
            Controls.ComboBox {
                id: activeToolComboBox
                Layout.fillWidth: true
                model: ["Freehand", "Rectangle", "Ellipse", "Line", "Arrow", "Text"]
                currentIndex: {
                    let tool = root.backend.activeTool;
                    if (tool && tool !== "") {
                        let idx = ["freehand", "rectangle", "ellipse", "line", "arrow", "text"].indexOf(tool);
                        if (idx >= 0) return idx;
                    }
                    let idx = ["freehand", "rectangle", "ellipse", "line", "arrow", "text"].indexOf(fullRoot.currentToolName);
                    return idx >= 0 ? idx : 0;
                }
                onActivated: {
                    fullRoot.currentToolName = currentValue.toLowerCase()
                    root.backend.setTool(currentValue.toLowerCase())
                }
            }
        }

        // Row 2b: Target Screen selection (always available)
        RowLayout {
            Layout.fillWidth: true
            
            PlasmaComponents.Label {
                text: "Target Screen:"
            }
            
            Controls.ComboBox {
                id: targetScreenCombo
                Layout.fillWidth: true
                model: root.backend.screenNames
                currentIndex: model.indexOf(root.backend.targetScreen)
                onActivated: {
                    Plasmoid.configuration.targetScreen = currentValue
                    root.backend.setTargetScreen(currentValue)
                }
                
                Connections {
                    target: root.backend
                    function onTargetScreenChanged() {
                        targetScreenCombo.currentIndex = targetScreenCombo.model.indexOf(root.backend.targetScreen)
                    }
                }
            }
        }

        // Grid: Color Swatches & Picker
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.Label {
                text: "Color:"
                Layout.alignment: Qt.AlignVCenter
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                // 6 Preset colors (Red, Orange, Yellow, Green, Blue, Violet)
                property var colors: ["#e63946", "#f4a261", "#e9c46a", "#2a9d8f", "#457b9d", "#8338ec"]
                property string activeColor: root.backend.hasSelection ? root.backend.selectedColor : "#e63946"

                Repeater {
                    model: parent.colors
                    Rectangle {
                        width: Kirigami.Units.gridUnit * 1.5
                        height: Kirigami.Units.gridUnit * 1.5
                        radius: 4
                        color: modelData
                        border.width: parent.activeColor === modelData ? 2 : 1
                        border.color: parent.activeColor === modelData ? Kirigami.Theme.highlightColor : "gray"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                root.backend.setColor(modelData)
                            }
                        }
                    }
                }

                // Custom Color Selector
                PlasmaComponents.Button {
                    icon.name: "color-picker"
                    text: "Custom"
                    onClicked: {
                        colorDialog.selectedColor = parent.activeColor
                        colorDialog.open()
                    }
                }
            }
        }

        // Row 3: Stroke Width
        RowLayout {
            Layout.fillWidth: true
            
            PlasmaComponents.Label {
                text: "Width:"
                width: Kirigami.Units.gridUnit * 3
            }
            
            PlasmaComponents.Slider {
                Layout.fillWidth: true
                from: 1
                to: 15
                stepSize: 1
                value: root.backend.hasSelection ? root.backend.selectedStrokeWidth : 2
                onMoved: {
                    root.backend.setStrokeWidth(value)
                }
            }

            PlasmaComponents.Label {
                text: Math.round(root.backend.hasSelection ? root.backend.selectedStrokeWidth : 2) + "px"
            }
        }

        // Row 4: Opacity
        RowLayout {
            Layout.fillWidth: true
            
            PlasmaComponents.Label {
                text: "Opacity:"
                width: Kirigami.Units.gridUnit * 3
            }
            
            PlasmaComponents.Slider {
                Layout.fillWidth: true
                from: 0.1
                to: 1.0
                stepSize: 0.05
                value: root.backend.hasSelection ? root.backend.selectedOpacity : 1.0
                onMoved: {
                    root.backend.setOpacity(value)
                }
            }

            PlasmaComponents.Label {
                text: Math.round((root.backend.hasSelection ? root.backend.selectedOpacity : 1.0) * 100) + "%"
            }
        }

        // Row 4.5: Glow
        RowLayout {
            Layout.fillWidth: true
            
            PlasmaComponents.Label {
                text: "Glow:"
                width: Kirigami.Units.gridUnit * 3
            }
            
            PlasmaComponents.Slider {
                Layout.fillWidth: true
                from: 0
                to: 30
                stepSize: 1
                value: root.backend.selectedGlow
                onMoved: {
                    root.backend.setGlow(value)
                }
            }

            PlasmaComponents.Label {
                text: Math.round(root.backend.selectedGlow) + "px"
            }
        }

        // Row 5: Border Radius (only if Rectangle tool is active or Rectangle shape is selected)
        RowLayout {
            Layout.fillWidth: true
            visible: fullRoot.isRectActive
            
            PlasmaComponents.Label {
                text: "Radius:"
                width: Kirigami.Units.gridUnit * 3
            }
            
            PlasmaComponents.Slider {
                Layout.fillWidth: true
                from: 0
                to: 50
                stepSize: 1
                value: root.backend.selectedBorderRadius
                onMoved: {
                    root.backend.setBorderRadius(value)
                }
            }

            PlasmaComponents.Label {
                text: Math.round(root.backend.selectedBorderRadius) + "px"
            }
        }

        // Font Config Row (only if Text tool is active or Text shape is selected)
        property bool isTextActive: (root.backend.hasSelection && root.backend.selectedType.toLowerCase() === "text") || (!root.backend.hasSelection && fullRoot.currentToolName === "text")
        
        ColumnLayout {
            Layout.fillWidth: true
            visible: parent.isTextActive
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                PlasmaComponents.Label {
                    text: "Font:"
                    width: Kirigami.Units.gridUnit * 3
                }
                
                Controls.ComboBox {
                    Layout.fillWidth: true
                    model: Qt.fontFamilies()
                    currentIndex: Qt.fontFamilies().indexOf(root.backend.selectedFontFamily)
                    onActivated: {
                        root.backend.setFontFamily(currentValue)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                PlasmaComponents.Label {
                    text: "Font Size:"
                    width: Kirigami.Units.gridUnit * 3
                }
                
                PlasmaComponents.Slider {
                    Layout.fillWidth: true
                    from: 10
                    to: 72
                    stepSize: 1
                    value: root.backend.selectedFontSize
                    onMoved: {
                        root.backend.setFontSize(value)
                    }
                }

                PlasmaComponents.Label {
                    text: Math.round(root.backend.selectedFontSize) + "px"
                }
            }
        }

        // Section: Shape List
        PlasmaComponents.Label {
            text: "Drawn Shapes Manager"
            font.bold: true
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            color: Kirigami.Theme.highlightColor
        }

        Controls.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            ListView {
                id: shapesListView
                model: root.backend.shapesList
                
                delegate: PlasmaComponents.ItemDelegate {
                    width: shapesListView.width
                    height: Kirigami.Units.gridUnit * 2
                    
                    background: Rectangle {
                        color: (modelData && modelData.selected)
                               ? Kirigami.Theme.highlightColor
                               : (hovered ? Kirigami.Theme.hoverColor : "transparent")
                        opacity: (modelData && modelData.selected) ? 0.3 : 1.0
                        radius: 4
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        // Shape Icon / Type Label
                        Kirigami.Icon {
                            source: {
                                let t = modelData && modelData.type ? modelData.type.toLowerCase() : "";
                                switch(t) {
                                    case "rectangle": return "draw-rectangle"
                                    case "ellipse": return "draw-ellipse"
                                    case "line": return "draw-line"
                                    case "arrow": return "draw-arrow"
                                    case "freehand": return "draw-freehand"
                                    case "text": return "draw-text"
                                    default: return "draw-freehand"
                                }
                            }
                            width: Kirigami.Units.iconSizes.small
                            height: Kirigami.Units.iconSizes.small
                        }

                        PlasmaComponents.Label {
                            text: (modelData && modelData.name) ? modelData.name : ("Shape " + (index + 1))
                            font.bold: modelData ? !!modelData.selected : false
                            Layout.fillWidth: true
                        }

                        // Lock / Unlock Button in list
                        PlasmaComponents.ToolButton {
                            icon.name: (modelData && modelData.locked) ? "object-locked" : "object-unlocked"
                            onClicked: {
                                root.backend.setShapeLocked(index, !(modelData && modelData.locked))
                            }
                        }

                        // Delete Button in list
                        PlasmaComponents.ToolButton {
                            icon.name: "edit-delete"
                            onClicked: {
                                root.backend.deleteShape(index)
                            }
                        }
                    }
                    
                    onClicked: {
                        root.backend.selectShape(index)
                    }
                }

                // Empty state indicator
                Text {
                    anchors.centerIn: parent
                    text: "No annotations on screen"
                    color: Kirigami.Theme.disabledTextColor
                    visible: shapesListView.count === 0
                }
            }
        }

        // Section: Keyboard Shortcuts collapsible header
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            MouseArea {
                id: shortcutsHeaderMouseArea
                Layout.fillWidth: true
                height: shortcutsHeaderLabel.implicitHeight + Kirigami.Units.smallSpacing * 2
                hoverEnabled: true

                property bool expanded: false

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Kirigami.Units.smallSpacing
                    anchors.rightMargin: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: shortcutsHeaderMouseArea.expanded ? "arrow-down" : "arrow-right"
                        width: Kirigami.Units.iconSizes.small
                        height: Kirigami.Units.iconSizes.small
                    }

                    PlasmaComponents.Label {
                        id: shortcutsHeaderLabel
                        text: "Keyboard Shortcuts"
                        font.bold: true
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: Kirigami.Theme.highlightColor
                        Layout.fillWidth: true
                    }
                }

                onClicked: {
                    shortcutsHeaderMouseArea.expanded = !shortcutsHeaderMouseArea.expanded
                }
            }
        }

        // Collapsible Content
        ColumnLayout {
            Layout.fillWidth: true
            visible: shortcutsHeaderMouseArea.expanded
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.Label {
                text: {
                    if (reassignmentNoticeText !== "") return reassignmentNoticeText;
                    if (recordingActionId !== "") return "Recording... Press keys, Esc to cancel";
                    return "Click on a shortcut button to change it.";
                }
                font.italic: true
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                color: {
                    if (reassignmentNoticeText !== "") return Kirigami.Theme.highlightColor;
                    if (recordingActionId !== "") return Kirigami.Theme.warningColor;
                    return Kirigami.Theme.disabledTextColor;
                }
                Layout.fillWidth: true
            }

            Controls.ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 12
                clip: true

                ColumnLayout {
                    width: parent.width - Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: root.backend.shortcuts

                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            PlasmaComponents.Label {
                                Layout.fillWidth: true
                                text: modelData.type === "global" ? "Global Shortcuts" : "Local Shortcuts (draw/select mode)"
                                font.bold: true
                                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                color: Kirigami.Theme.disabledTextColor
                                visible: index === 0 || root.backend.shortcuts[index - 1].type !== modelData.type
                                Layout.topMargin: (index > 0 && root.backend.shortcuts[index - 1].type !== modelData.type) ? Kirigami.Units.smallSpacing * 1.5 : 0
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                PlasmaComponents.Label {
                                    text: modelData.name
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                PlasmaComponents.Button {
                                    id: shortcutButton
                                    text: {
                                        if (recordingActionId === modelData.id) {
                                            return "Press keys..."
                                        }
                                        return modelData.shortcut !== "" ? modelData.shortcut : "None"
                                    }
                                    font.bold: recordingActionId === modelData.id
                                    font.capitalization: Font.MixedCase
                                    checkable: true
                                    checked: recordingActionId === modelData.id
                                    
                                    onClicked: {
                                        if (recordingActionId === modelData.id) {
                                            recordingActionId = ""
                                        } else {
                                            recordingActionId = modelData.id
                                            shortcutButton.forceActiveFocus()
                                        }
                                    }

                                    Keys.onPressed: (event) => {
                                        if (recordingActionId !== modelData.id) return;
                                        
                                        // Cancel on Escape
                                        // Use Qt.Key_Escape, etc.
                                        if (event.key === Qt.Key_Escape) {
                                            recordingActionId = "";
                                            event.accepted = true;
                                            return;
                                        }
                                        
                                        // Ignore solo modifier presses
                                        let key = event.key;
                                        if (key === Qt.Key_Control || key === Qt.Key_Shift || key === Qt.Key_Alt || key === Qt.Key_Meta ||
                                            key === Qt.Key_Super_L || key === Qt.Key_Super_R || key === Qt.Key_AltGr ||
                                            key === Qt.Key_Hyper_L || key === Qt.Key_Hyper_R) {
                                            event.accepted = true;
                                            return;
                                        }
                                        
                                        let newSeqString = root.backend.formatKeySequence(event.key, event.modifiers);
                                        let prevActionName = "";
                                        for (let i = 0; i < root.backend.shortcuts.length; ++i) {
                                            let s = root.backend.shortcuts[i];
                                            if (s.shortcut === newSeqString && s.id !== recordingActionId) {
                                                prevActionName = s.name;
                                                break;
                                            }
                                        }
                                        
                                        root.backend.changeShortcut(recordingActionId, newSeqString);
                                        
                                        if (prevActionName !== "") {
                                            reassignmentNoticeText = "Reassigned from " + prevActionName;
                                            reassignmentNoticeTimer.restart();
                                        } else {
                                            reassignmentNoticeText = "";
                                        }
                                        
                                        recordingActionId = "";
                                        event.accepted = true;
                                    }
                                }

                                PlasmaComponents.ToolButton {
                                    icon.name: "edit-clear"
                                    Controls.ToolTip.text: "Reset to default"
                                    Controls.ToolTip.visible: hovered
                                    enabled: modelData.shortcut !== modelData.defaultShortcut
                                    onClicked: {
                                        root.backend.changeShortcut(modelData.id, modelData.defaultShortcut)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
