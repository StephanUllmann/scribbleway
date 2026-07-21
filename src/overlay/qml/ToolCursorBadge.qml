import QtQuick

Item {
    id: root
    width: 28
    height: 28

    // freehand | rectangle | ellipse | line | arrow | text
    property string tool: "freehand"
    property color accent: "#e63946"
    property color chipColor: Qt.rgba(0.08, 0.09, 0.12, 0.82)
    property color chipBorder: Qt.rgba(1, 1, 1, 0.35)

    // Never intercept pointer events — shapes / draw MouseArea stay authoritative
    enabled: false

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: root.chipColor
        border.color: root.chipBorder
        border.width: 1
    }

    // --- Glyphs (pure geometry; no icon theme) ---
    Item {
        id: glyph
        anchors.centerIn: parent
        width: 16
        height: 16

        // Freehand: short scribble
        Canvas {
            anchors.fill: parent
            visible: root.tool === "freehand"
            property color accent: root.accent
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = accent;
                ctx.lineWidth = 1.8;
                ctx.lineCap = "round";
                ctx.beginPath();
                ctx.moveTo(2, 12);
                ctx.quadraticCurveTo(5, 2, 8, 8);
                ctx.quadraticCurveTo(11, 14, 14, 4);
                ctx.stroke();
            }
            onVisibleChanged: if (visible) requestPaint()
            onAccentChanged: requestPaint()
            Component.onCompleted: requestPaint()
        }

        // Rectangle
        Rectangle {
            visible: root.tool === "rectangle"
            anchors.centerIn: parent
            width: 12
            height: 10
            radius: 1
            color: "transparent"
            border.color: root.accent
            border.width: 1.6
        }

        // Ellipse
        Rectangle {
            visible: root.tool === "ellipse"
            anchors.centerIn: parent
            width: 13
            height: 10
            radius: width / 2
            color: "transparent"
            border.color: root.accent
            border.width: 1.6
        }

        // Line
        Rectangle {
            visible: root.tool === "line"
            width: 14
            height: 1.8
            radius: 1
            color: root.accent
            anchors.centerIn: parent
            rotation: -32
        }

        // Arrow (stem + head via two rects)
        Item {
            visible: root.tool === "arrow"
            anchors.fill: parent
            Rectangle {
                width: 12
                height: 1.8
                radius: 1
                color: root.accent
                x: 2
                y: 7
                rotation: -32
                transformOrigin: Item.Left
            }
            // simple chevron head
            Canvas {
                anchors.fill: parent
                visible: parent.visible
                property color accent: root.accent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = accent;
                    ctx.lineWidth = 1.6;
                    ctx.lineCap = "round";
                    ctx.lineJoin = "round";
                    ctx.beginPath();
                    ctx.moveTo(9, 3);
                    ctx.lineTo(14, 4);
                    ctx.lineTo(11, 9);
                    ctx.stroke();
                }
                onVisibleChanged: if (visible) requestPaint()
                onAccentChanged: requestPaint()
                Component.onCompleted: requestPaint()
            }
        }

        // Text: "T"
        Text {
            visible: root.tool === "text"
            anchors.centerIn: parent
            text: "T"
            color: root.accent
            font.pixelSize: 14
            font.bold: true
        }
    }

    // Tiny shortcut hint in bottom-right of chip
    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 2
        text: {
            switch (root.tool) {
            case "arrow": return "A";
            case "rectangle": return "R";
            case "freehand": return "F";
            case "ellipse": return "E";
            case "line": return "L";
            case "text": return "T";
            default: return "";
            }
        }
        color: Qt.rgba(1, 1, 1, 0.75)
        font.pixelSize: 7
        font.bold: true
    }
}
