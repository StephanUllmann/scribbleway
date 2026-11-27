import QtQuick
import ".."
import "../shapes"
import "../shapes/RoughPathGenerator.js" as RoughPathGenerator

// The selected Shape, or the next one of `type` when nothing is selected, drawn by the
// canvas's own delegates: what shows here is how the Shape is drawn, roughness, glow and all.
Rectangle {
    id: well
    property string type: "rectangle"
    property var roughnessNames: []

    Theme { id: theme }

    radius: 10
    color: theme.well
    border.color: theme.line
    clip: true

    readonly property var styleKeys: ["color", "strokeWidth", "opacity", "fontFamily", "fontSize",
                                      "borderRadius", "roughness", "glow", "fillColor", "fillOpacity"]

    function specimenShape(type, w, h) {
        const s = controller.selection
        // Without a selection, exactly the keys the next Shape of this type will carry.
        const style = controller.hasSelection ? s : controller.styleForNewShape(type)
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

    readonly property var specimen: specimenShape(type, width, height)
    onSpecimenChanged: controller.setSpecimenShape(specimen)

    readonly property string readout: {
        const s = specimen
        const pct = v => Math.round(v * 100) + "%"
        if (type === "text") {
            return s.fontFamily + " · " + Math.round(s.fontSize) + " px · " + pct(s.opacity)
        }
        let parts = ["W " + Math.round(s.strokeWidth) + " px", pct(s.opacity)]
        if ((type === "rectangle" || type === "ellipse") && s.fillColor !== "transparent") {
            parts.push("fill " + pct(s.fillOpacity))
        }
        parts.push(roughnessNames[Math.min(2, Math.max(0, Math.round(s.roughness)))])
        return parts.join(" · ")
    }

    readonly property string name: {
        if (!controller.hasSelection) return "New " + type
        const t = controller.selection.type
        return t.charAt(0).toUpperCase() + t.slice(1) + " " + (controller.selection.selectedIndex + 1)
             + (controller.hasMultiSelection ? " and others" : "")
    }

    Repeater {
        model: controller.specimenModel
        delegate: ShapeLoader {
            type: model.type
            enabled: false
        }
    }

    Label {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 10
        text: well.name
        color: controller.hasSelection ? theme.ink : theme.mute
        font.weight: Font.DemiBold
    }

    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 6
        spacing: 4
        visible: controller.hasSelection
        TextButton { text: "Raise"; onClicked: controller.raiseSelected() }
        TextButton { text: "Lower"; onClicked: controller.lowerSelected() }
        TextButton { text: "Delete"; tone: theme.danger; onClicked: controller.deleteSelected() }
    }

    Label {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 10
        text: well.readout.toUpperCase()
        color: theme.mute
        font.family: theme.mono
        font.pixelSize: theme.small
        font.letterSpacing: 0.8
        font.features: { "tnum": 1 }
    }
}
