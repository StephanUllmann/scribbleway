# Glow in Live Preview

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Live preview shapes drawn while the pointer is down must render the same glow as finalized shapes when `controller.defaultGlow > 0`, so the preview matches the committed look.

**Architecture:** Reuse the existing `MultiEffect` shadow-glow pattern from `BaseShape.qml` (lines 10–28). For each live-preview block in `main.qml` (lines 294–434), introduce an outer `Item` (visible/opacity/geometry) containing (1) a `MultiEffect` and (2) a **group content** `Item` that is the MultiEffect source. Put today’s clean stroke/fill inside that group. Sibling plans (live-preview roughness) may add `RoughStroke` as another child of the same group — glow then wraps the whole group, not only the clean stroke. No C++/model/D-Bus changes.

**Tech Stack:** QML, `QtQuick.Effects.MultiEffect` (already used by finalized shapes).

---

## Motivation

Finalized shapes glow via `BaseShape.qml`:

```qml
MultiEffect {
    id: glowEffect
    anchors.fill: shapeContent
    source: shapeContent
    visible: baseShapeRoot.modelGlow > 0
    shadowEnabled: true
    shadowColor: baseShapeRoot.modelColor
    shadowBlur: baseShapeRoot.modelGlow / 30.0
    shadowHorizontalOffset: 0
    shadowVerticalOffset: 0
    autoPaddingEnabled: true
}
Item {
    id: shapeContent
    anchors.fill: parent
    visible: baseShapeRoot.modelGlow === 0
}
```

Live previews in `main.qml` (freehand, rectangle, ellipse, line, arrow) are plain `Shape` / `Rectangle` nodes. They ignore `controller.defaultGlow`, so during a drag the stroke looks flat and “pops” into a glow on mouse-up. Preview should match the defaults that `finalizeShape()` already stamps (`"glow": controller.defaultGlow` around line 128).

---

## Design

### Constraints

- Glow range and formula stay identical to finalized shapes: `shadowBlur = defaultGlow / 30.0`, color = stroke color (`controller.defaultColor`), offsets 0, `autoPaddingEnabled: true`.
- When `defaultGlow === 0`, behavior and layering must match today (no MultiEffect cost beyond `visible: false`).
- Opacity stays on the outer wrapper (`opacity: controller.defaultOpacity`), not inside MultiEffect, matching how BaseShape leaves opacity on the shape root.
- No roughness / fill / cursor changes in this plan — glow only.
- Text tool has no rubber-band preview (opens editor); skip it.

### Approach

For each of the five preview blocks (lines 294–434), introduce:

```
Item {                        // outer: visible, opacity, geometry
    MultiEffect { … source: <tool>PreviewContent … }
    Item {
        id: <tool>PreviewContent   // MultiEffect source; visible when glow === 0
        // existing clean Shape / Rectangle  (and later RoughStroke siblings)
    }
}
```

1. Outer `Item` owns `visible` / geometry / `opacity` currently on the preview root.
2. `MultiEffect` sibling wired like BaseShape, driven by `controller.defaultGlow` and `controller.defaultColor`.
3. Group content `Item` (`*PreviewContent`) is the MultiEffect `source`, with `visible: controller.defaultGlow === 0` (same hide-when-glowing trick as BaseShape’s `shapeContent`).
4. Move the existing visual **inside** the group content — do **not** make the bare `Shape`/`Rectangle` the MultiEffect source. That leaves room for a future `RoughStroke` sibling under the same group so glow + roughness compose correctly (MultiEffect captures the whole group).

Do **not** extract a shared component unless a sibling plan already did — keep the change local to `main.qml` and mirror BaseShape literally so the two stay easy to diff.

**Coordination with live-preview roughness:** That plan keeps clean fill, hides clean stroke when `defaultRoughness > 0`, and overlays `RoughStroke` using a stable `previewSeed`. Implementers of either plan MUST keep the group-content `Item` as the glow source and place RoughStroke **inside** it, not as a peer of the outer wrapper. Order of landing does not matter if both respect this nesting.

### Shared MultiEffect snippet (paste per preview)

```qml
MultiEffect {
    anchors.fill: <tool>PreviewContent
    source: <tool>PreviewContent
    visible: controller.defaultGlow > 0
    shadowEnabled: true
    shadowColor: controller.defaultColor
    shadowBlur: controller.defaultGlow / 30.0
    shadowHorizontalOffset: 0
    shadowVerticalOffset: 0
    autoPaddingEnabled: true
}
```

### Geometry notes

| Preview | Current root | Outer wrapper geometry | Group content |
|---|---|---|---|
| Freehand | `Shape { anchors.fill: parent }` | `Item { anchors.fill: parent }` | `Item { id: freehandPreviewContent; anchors.fill: parent }` holding the `Shape` (+ future RoughStroke) |
| Rectangle | `Rectangle { x/y/w/h }` | `Item { x: previewX; y: previewY; width: previewW; height: previewH }` | `Item { id: rectPreviewContent; anchors.fill: parent }` holding the `Rectangle` (+ future RoughStroke). Geometry lives on outer, not on `Rectangle`. |
| Ellipse | `Shape { anchors.fill: parent }` | `Item { anchors.fill: parent }` | `Item { id: ellipsePreviewContent; … }` holding the `Shape` |
| Line | `Shape { anchors.fill: parent }` | `Item { anchors.fill: parent }` | `Item { id: linePreviewContent; … }` holding the `Shape` |
| Arrow | `Shape { id: arrowPreviewShape; …trig… }` | Outer `Item` owns `visible`/`opacity`/`anchors` | Group `Item { id: arrowPreviewContent }`. Keep `id: arrowPreviewShape` and trig props on the inner `Shape` so path bindings stay valid. MultiEffect sources `arrowPreviewContent`, not the Shape alone. |

### Import

Add once at top of `main.qml`:

```qml
import QtQuick.Effects
```

(`QtQuick.Shapes` already present.)

### Data model / D-Bus / Excalidraw

None. `defaultGlow` and per-shape `glow` already exist (`OverlayController::defaultGlow`, `GlowRole`, finalize path). This is pure preview rendering.

---

## File Structure

| File | Change |
|---|---|
| `src/overlay/qml/main.qml` | `import QtQuick.Effects`; wrap five live-preview blocks (≈294–434) with outer Item + MultiEffect + **group content Item** (clean visual inside; RoughStroke-ready) |
| `src/overlay/qml/shapes/BaseShape.qml` | **Read-only reference** — do not modify |

No tests C++ changes required (QML-only visual). Manual verification below.

---

## Step-by-step implementation

### Task 1: Import + freehand / rectangle / ellipse previews

**Files:**
- Modify: `src/overlay/qml/main.qml`

- [ ] **Step 1: Add Effects import**

Near the top of `main.qml` (after existing imports, ~line 4):

```qml
import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls
import QtQuick.Shapes
import QtQuick.Effects
```

- [ ] **Step 2: Wrap Freehand Live Preview (≈294–311)**

Replace the bare `Shape { … }` with:

```qml
// Freehand Live Preview
Item {
    anchors.fill: parent
    visible: isDrawing && activeDrawTool === "freehand"
    opacity: controller.defaultOpacity

    MultiEffect {
        anchors.fill: freehandPreviewContent
        source: freehandPreviewContent
        visible: controller.defaultGlow > 0
        shadowEnabled: true
        shadowColor: controller.defaultColor
        shadowBlur: controller.defaultGlow / 30.0
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    // Group content = MultiEffect source. Clean stroke lives here;
    // live-preview roughness may add RoughStroke as a sibling inside this Item.
    Item {
        id: freehandPreviewContent
        anchors.fill: parent
        visible: controller.defaultGlow === 0

        Shape {
            anchors.fill: parent

            ShapePath {
                strokeColor: controller.defaultColor
                strokeWidth: controller.defaultStrokeWidth
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathPolyline {
                    id: freehandPreviewPath
                    path: []
                }
            }
        }
    }
}
```

**Critical:** Keep `id: freehandPreviewPath` on `PathPolyline` — draw handlers assign `freehandPreviewPath.path` (search `main.qml` for that id before editing; do not rename).

- [ ] **Step 3: Wrap Rectangle Live Preview (≈313–328)**

```qml
// Rectangle Live Preview
Item {
    visible: isDrawing && activeDrawTool === "rectangle"
    x: previewX
    y: previewY
    width: previewW
    height: previewH
    opacity: controller.defaultOpacity

    MultiEffect {
        anchors.fill: rectPreviewContent
        source: rectPreviewContent
        visible: controller.defaultGlow > 0
        shadowEnabled: true
        shadowColor: controller.defaultColor
        shadowBlur: controller.defaultGlow / 30.0
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Item {
        id: rectPreviewContent
        anchors.fill: parent
        visible: controller.defaultGlow === 0

        Rectangle {
            anchors.fill: parent
            border.color: controller.defaultColor
            border.width: controller.defaultStrokeWidth
            color: {
                let c = Qt.color(controller.defaultColor);
                return Qt.rgba(c.r, c.g, c.b, 0.12);
            }
            radius: controller.defaultBorderRadius
        }
        // RoughStroke sibling (roughness plan) goes here inside rectPreviewContent
    }
}
```

Same outer-`Item` + MultiEffect + group-content pattern as freehand. Group id e.g. `ellipsePreviewContent`; keep all `PathArc` geometry bindings (`previewX/Y/W/H`) unchanged on the inner `Shape`/`ShapePath`.

- [ ] **Step 5: Smoke-check QML loads**

Run overlay (or `qmlcachegen` / app start). Expected: no “MultiEffect is not a type” / missing module errors. Runtime dep `qml6-module-qtquick-effects` already required by finalized glow.

---

### Task 2: Line + arrow previews

**Files:**
- Modify: `src/overlay/qml/main.qml`

- [ ] **Step 1: Wrap Line Live Preview (≈367–385)**

```qml
// Line Live Preview
Item {
    anchors.fill: parent
    visible: isDrawing && activeDrawTool === "line"
    opacity: controller.defaultOpacity

    MultiEffect {
        anchors.fill: linePreviewContent
        source: linePreviewContent
        visible: controller.defaultGlow > 0
        shadowEnabled: true
        shadowColor: controller.defaultColor
        shadowBlur: controller.defaultGlow / 30.0
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Item {
        id: linePreviewContent
        anchors.fill: parent
        visible: controller.defaultGlow === 0

        Shape {
            anchors.fill: parent

            ShapePath {
                strokeColor: controller.defaultColor
                strokeWidth: controller.defaultStrokeWidth
                fillColor: "transparent"

                startX: drawStartPoint.x
                startY: drawStartPoint.y
                PathLine {
                    x: drawStartPoint.x + previewW
                    y: drawStartPoint.y + previewH
                }
            }
        }
    }
}
```

Keep `id: arrowPreviewShape` and all angle/len/base trig properties on the **inner** `Shape` so existing `PathLine` bindings (`arrowPreviewShape.baseX`, etc.) need no rewrite. MultiEffect sources the **group** `arrowPreviewContent`, not the Shape alone:

```qml
// Arrow Live Preview
Item {
    anchors.fill: parent
    visible: isDrawing && activeDrawTool === "arrow"
    opacity: controller.defaultOpacity

    MultiEffect {
        anchors.fill: arrowPreviewContent
        source: arrowPreviewContent
        visible: controller.defaultGlow > 0
        shadowEnabled: true
        shadowColor: controller.defaultColor
        shadowBlur: controller.defaultGlow / 30.0
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Item {
        id: arrowPreviewContent
        anchors.fill: parent
        visible: controller.defaultGlow === 0

        Shape {
            id: arrowPreviewShape
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            property real angle: Math.atan2(previewH, previewW)
            property real len: 10 + controller.defaultStrokeWidth * 1.5
            property real halfAng: Math.PI / 6
            property real tx: drawStartPoint.x + previewW
            property real ty: drawStartPoint.y + previewH
            property real lineLength: Math.sqrt(Math.pow(previewW, 2) + Math.pow(previewH, 2))
            property real stemLength: Math.max(0, lineLength - len * Math.cos(halfAng))
            property real baseX: drawStartPoint.x + stemLength * Math.cos(angle)
            property real baseY: drawStartPoint.y + stemLength * Math.sin(angle)

            // … existing two ShapePath blocks unchanged …
        }
        // RoughStroke sibling (roughness plan) goes here inside arrowPreviewContent
    }
}
```

- [ ] **Step 3: Grep for broken preview ids**

```bash
rg -n "freehandPreviewPath|arrowPreviewShape" src/overlay/qml/main.qml
```

Expected: `freehandPreviewPath` still the `PathPolyline` id; draw code that sets `.path` still resolves. `arrowPreviewShape` still the content `Shape`.

---

### Task 3: Manual verification

- [ ] **Step 1: Glow on (default)**

1. Set applet Glow slider > 0 (default is currently 10 in `overlaycontroller.h` `m_defaultGlow`).
2. Enter draw mode; drag rectangle, ellipse, line, arrow, freehand.
3. **Expected:** Preview stroke shows colored soft halo matching finalized shapes; on release, committed shape glow looks continuous (no pop-in).

- [ ] **Step 2: Glow off**

1. Set Glow slider to 0.
2. Drag each tool again.
3. **Expected:** Sharp preview only; no double-draw, no residual blur padding artifacts.

- [ ] **Step 3: Opacity + color**

1. Lower opacity slider; change color; glow > 0.
2. **Expected:** Glow color tracks stroke color; overall preview opacity tracks `defaultOpacity` (glow dims with the shape, not full-strength under a faded stroke).

- [ ] **Step 4: Abort / tool switch mid-drag**

Abort or switch tool while drawing. **Expected:** preview (and its MultiEffect) disappears; no stuck glow layer.

---

## Test plan

| Case | How | Pass |
|---|---|---|
| Glow > 0 all 5 tools | Manual drag | Halo visible during drag |
| Glow === 0 | Slider 0, drag | Identical to pre-change plain preview |
| Commit continuity | Release after drag | No visible glow jump |
| Id stability | `rg freehandPreviewPath` | Path updates still work |
| Module present | App start | No QML type errors for `MultiEffect` |
| Automated | N/A | No C++ contract change; existing `defaultGlow` tests in `tests/shapesmodeltest.cpp` remain green without edits |

Optional follow-up (out of scope): screenshot/visual regression harness for QML previews.

---

## Excalidraw compatibility impact

**None.** Preview-only QML. Clipboard `convertToExcalidraw` / `convertFromExcalidraw` and shape `glow` keys are untouched. Pasted Excalidraw elements still default glow `0` on the model; live preview still uses `controller.defaultGlow` for *new* draws only (same as finalize).

---

## Acceptance criteria

1. With `controller.defaultGlow > 0`, freehand / rectangle / ellipse / line / arrow live previews render a MultiEffect glow using the same formula as `BaseShape.qml` (`shadowBlur = glow/30`, `shadowColor = defaultColor`, zero offsets, auto padding).
2. With `controller.defaultGlow === 0`, previews render exactly as today (group content visible, MultiEffect hidden).
3. Each preview uses nesting `outer Item → MultiEffect + group content Item (*PreviewContent) → clean visual` so a future RoughStroke can sit inside the group and be captured by glow.
4. `import QtQuick.Effects` added to `main.qml`; no new files required.
5. `freehandPreviewPath` and arrow trigonometry bindings (`arrowPreviewShape.*`) keep working.
6. No C++ / D-Bus / applet / model API changes.
7. No Excalidraw serialization changes.
8. Committing a shape after a glowing preview does not produce a noticeable glow pop-in/pop-out relative to the preview.

---

## Non-goals

- Live-preview roughness implementation (separate plan) — only the nesting contract above.
- Text-tool preview glow.
- Refactoring BaseShape to share a component with previews (optional cleanup later).
- Changing default glow value or slider UX.
- Glow on selection chrome / handles.
