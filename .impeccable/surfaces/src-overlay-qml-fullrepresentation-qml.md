---
version: 1
slug: "src-overlay-qml-fullrepresentation-qml"
primary_target: "src/overlay/qml/FullRepresentation.qml"
related_targets: ["src/overlay/qml/TrayPopup.qml","src/overlay/qml/ColorPickerPanel.qml"]
---

# Tray Popup

Mode: Operate. A 432px-wide anchored popup beside a live drawing canvas. Main job: restyle the selected Shape; second: pick the next tool; third: manage the shape list. Own fixed dark look on every platform. Rarely-used settings (glow, smoothing, corner radius, target screen) live in a More tab. Constraints: plain QtQuick/Controls only, dropdowns in-window, `selectModeButton` and `shapesListView` objectNames kept for tests.

## Direction contract

THESIS: The selected Shape is shown as a live specimen that redraws as any control moves; the popup refuses the category's flat column of labelled sliders where the result is only visible on the canvas behind it.

OWN-WORLD: Ink-black ground (#111), one step-up surface (#1c1c1c), paper-white ink (#f1efe9), warm grey secondary (≥4.5:1), one lavender accent (#a99bff) for current tool, slider fill and focus. Preset colours are the only other colour. One body size; rank by weight. Small mono uppercase only for measurements and hotkeys.

STORY: The user opens the popup, sees the selected Shape exactly as it is drawn, nudges colour/width/fill/roughness and watches the specimen change, then closes it. Hotkey letters printed on tools and swatches teach the keyboard.

FIRST VIEWPORT: Top: Draw/Select segmented, undo/redo icon buttons. Row of six tool buttons with hotkey letters, current one lavender. Tabs Style · Shapes (count) · More. Style tab: specimen well (~118px) with the Shape drawn at its real style, name and Raise/Lower/Delete inside it, mono readout line; then aligned rows (78px label column, control, 40px value column): Stroke, Width, Opacity, Fill, Fill opacity, Roughness segmented; Font and Size when text.

FORM: Live Specimen (variable-font specimen fused), position: bolder re-roll lead; seed:c035d991ff8a4c6c. Signature move: the specimen well.

FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, DESIGN.md, and every shipping raster carrying its provenance
