# Live Preview Roughness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make live drawing previews use the same RoughPathGenerator/RoughStroke rendering as finalized shapes so releasing the mouse no longer jumps from clean Qt primitives to rough strokes.

**Architecture:** Keep the finalized-shape dual-path pattern (`clean primitive when roughness === 0`, `RoughStroke` when roughness > 0). Apply it inside each live-preview block in `main.qml`. Allocate a stable `previewSeed` on press and reuse it in `finalizeShape()` so rough geometry does not re-roll on commit. Nest clean stroke/fill and `RoughStroke` as **siblings inside a `*PreviewContent` group Item** so the sibling glow-preview plan’s `MultiEffect` (source = that group) captures rough strokes too.

**Tech Stack:** QML / QtQuick / QtQuick.Shapes / RoughPathGenerator.js / RoughStroke.qml

## Global Constraints

- No C++ / D-Bus / model role changes — preview is pure QML.
- No changes to `RoughPathGenerator.js` or `RoughStroke.qml` APIs.
- Match finalized-shape visual rules (fill stays clean; stroke switches clean ↔ rough).
- When `controller.defaultRoughness === 0`, previews must remain visually identical to today’s clean primitives.
- Seed must be stable for one drag and written into the finalized shape (no re-jitter on commit).
- Freehand, rectangle, ellipse, line, arrow in scope; text has no stroke preview.
- Do not touch applet UI or roughness defaults (owned by other plans).
- **Glow composition (required):** `RoughStroke` MUST be a child of `*PreviewContent` (the MultiEffect source group from `docs/superpowers/plans/2026-07-21-glow-preview.md`), never a peer of the outer wrapper / MultiEffect. Order of landing either plan does not matter if both respect this nesting.

---

## File Structure & Decomposition

| File | Responsibility |
|------|----------------|
| `src/overlay/qml/main.qml` | Import rough modules; `previewSeed` lifecycle; dual-path previews nested under `*PreviewContent` |
| `src/overlay/qml/shapes/RoughPathGenerator.js` | **Read-only** |
| `src/overlay/qml/shapes/RoughStroke.qml` | **Read-only** |
| `src/overlay/qml/shapes/{Rectangle,Ellipse,Line,Arrow,Freehand}Shape.qml` | **Reference only** — dual-path pattern |
| `docs/superpowers/plans/2026-07-21-glow-preview.md` | **Coordination** — outer Item + MultiEffect + `*PreviewContent` |

No new files.

### Existing finalized dual-path (mirror this)

**Line (`LineShape.qml:18-48`):**
```qml
Shape { visible: root.modelRoughness === 0 /* clean PathLine */ }
RoughStroke {
    strokes: root.modelRoughness > 0
        ? RoughPathGenerator.getSketchyLine(...)
        : []
    strokeColor: root.modelColor
    strokeWidth: root.modelStrokeWidth
    strokeOpacity: root.modelOpacity
}
```

**Rectangle (`RectangleShape.qml:18-46`):** fill always on; `border.width` is `0` when rough; `RoughStroke` + `getSketchyRectangle(...)`.

**Ellipse (`EllipseShape.qml:18-66`):** fill stays; clean `strokeColor` → `"transparent"` when rough; `getSketchyEllipse(...)`.

**Arrow (`ArrowShape.qml:35-91`):** clean stem + filled triangle when rough===0; rough uses `getSketchyArrow` (open chevron strokes, not filled triangle).

**Freehand (`FreehandShape.qml:56-82`):** clean `PathPolyline` vs `getSketchyFreehand`.

**Seed today (`main.qml:129`):**
```javascript
"seed": Math.floor(Math.random() * 1000000) + 1
```
Move generation to press-time.

### Generator signatures (do not change)

```javascript
getSketchyLine(x1, y1, x2, y2, roughness, seed) -> strokes[]
getSketchyRectangle(x, y, w, h, roughness, seed, borderRadius) -> strokes[]
getSketchyEllipse(x, y, w, h, roughness, seed) -> strokes[]
getSketchyArrow(fromX, fromY, toX, toY, roughness, seed, arrowLength) -> strokes[]
getSketchyFreehand(points, roughness, seed) -> strokes[]
// roughness === 0 → []
```

`RoughStroke` props: `strokes`, `strokeColor`, `strokeWidth`, `strokeOpacity`.

### Nesting contract with glow-preview

Target structure per tool (after one or both plans):

```
Item {                              // outer: visible, opacity, geometry
    MultiEffect {                   // ONLY if glow-preview already landed
        source: <tool>PreviewContent
        visible: controller.defaultGlow > 0
        /* shadow* props — do not edit in this plan */
    }
    Item {
        id: <tool>PreviewContent    // MultiEffect source group
        // glow plan sets: visible: controller.defaultGlow === 0
        //   (MultiEffect still samples the item when hidden)

        // clean Shape / Rectangle  (hide stroke when roughness > 0)
        RoughStroke { ... }         // THIS PLAN — sibling inside the group
    }
}
```

**Landing order:**

| Glow already in tree? | This plan does |
|----------------------|----------------|
| Yes — outer + MultiEffect + `*PreviewContent` with clean visual inside | Add `RoughStroke` **inside** `*PreviewContent`; adjust clean stroke visibility/border for roughness. Do not add a second outer wrapper. |
| No | Introduce lightweight outer `Item` + `*PreviewContent` holding clean + `RoughStroke`. Do **not** add MultiEffect (glow plan owns that). Prefer the same ids (`freehandPreviewContent`, `rectPreviewContent`, `ellipsePreviewContent`, `linePreviewContent`, `arrowPreviewContent`) so glow can wrap later without renames. |

**Opacity:** Put `opacity: controller.defaultOpacity` on the **outer** Item only. Child clean shapes and `RoughStroke` use full opacity (`strokeOpacity: 1.0`, no child `opacity`) so glow + rough do not double-multiply alpha. (If implementing before glow and keeping a flat structure temporarily, `strokeOpacity: controller.defaultOpacity` is OK — but once nested under an opaque outer, switch children to `1.0`.)

**Rectangle coordinate space:** Glow (and this plan when introducing outer) puts geometry on the outer Item (`x: previewX; y: previewY; width: previewW; height: previewH`). Inside that local space, call:

```javascript
RoughPathGenerator.getSketchyRectangle(0, 0, width, height, roughness, previewSeed, borderRadius)
```

not absolute `previewX/Y`. Freehand / ellipse / line / arrow outers stay `anchors.fill: parent` (full window), so those generators keep **absolute** canvas coordinates (`previewX/Y`, `drawStartPoint`, `activePoints`).

---

### Task 1: Preview seed lifecycle + imports

Wire rough modules into `main.qml` and make seed stable across press → drag → finalize.

**Files:**
- Modify: `src/overlay/qml/main.qml:1-5` (imports)
- Modify: `src/overlay/qml/main.qml:25-29` (preview properties)
- Modify: `src/overlay/qml/main.qml:248-260` (`onPressed`)
- Modify: `src/overlay/qml/main.qml:113-178` (`finalizeShape`)

**Interfaces:**
- Consumes: existing preview geometry props
- Produces: `property int previewSeed`; assigned on press; written by `finalizeShape`

- [ ] **Step 1: Add imports**

At top of `src/overlay/qml/main.qml`:

```qml
import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls
import QtQuick.Shapes
import "shapes"
import "shapes/RoughPathGenerator.js" as RoughPathGenerator
```

(`import "shapes"` makes `RoughStroke` available as a type. `QtQuick.Effects` is glow-plan only — do not add it here.)

If `import "shapes"` fails at runtime, confirm `main.qml` and `shapes/` share the same QML root:

```bash
ls src/overlay/qml/shapes/RoughStroke.qml
rg -n "main.qml|qml/shapes|importPath|QML_IMPORT|qt_add_qml_module" src/overlay/main.cpp src/overlay/CMakeLists.txt
```

Shape delegates already load from that directory via Repeater/Loader; sibling `import "shapes"` from `main.qml` is the expected fix if the engine needs an explicit directory import.

- [ ] **Step 2: Add `previewSeed` property**

Around `main.qml:25-29`:

```qml
    // Live preview properties
    property real previewX: 0
    property real previewY: 0
    property real previewW: 0
    property real previewH: 0
    property int previewSeed: 1
```

- [ ] **Step 3: Generate seed on press**

In draw `MouseArea` `onPressed` (`main.qml:248-260`):

```qml
        onPressed: (mouse) => {
            isDrawing = true;
            drawStartPoint = Qt.point(mouse.x, mouse.y);
            previewSeed = Math.floor(Math.random() * 1000000) + 1;

            if (activeDrawTool === "freehand") {
                activePoints = [drawStartPoint];
            } else {
                previewX = mouse.x;
                previewY = mouse.y;
                previewW = 0;
                previewH = 0;
            }
        }
```

- [ ] **Step 4: Finalize with press-time seed**

In `finalizeShape()` (`main.qml:120-130`), replace random seed:

```javascript
        let shape = {
            "type": activeDrawTool,
            "color": controller.defaultColor,
            "strokeWidth": controller.defaultStrokeWidth,
            "opacity": controller.defaultOpacity,
            "selected": false,
            "locked": false,
            "roughness": controller.defaultRoughness,
            "glow": controller.defaultGlow,
            "seed": previewSeed
        };
```

- [ ] **Step 5: Commit**

```bash
git add src/overlay/qml/main.qml
git commit -m "feat: stable previewSeed for live draw gestures"
```

---

### Task 2: Dual-path live previews (all tools)

Replace clean-only live previews (`main.qml:291-434`) with dual-path rendering nested under `*PreviewContent`.

**Files:**
- Modify: `src/overlay/qml/main.qml:291-434` (`// --- LIVE PREVIEW SHAPES ---` block)
- Modify: `src/overlay/qml/main.qml:265-270` (freehand path update guard)

**Interfaces:**
- Consumes: `previewSeed` (Task 1); `controller.defaultRoughness`, `defaultColor`, `defaultStrokeWidth`, `defaultOpacity`, `defaultBorderRadius`; preview geometry
- Produces: Rough live previews for freehand, rectangle, ellipse, line, arrow inside glow-compatible groups

**Before editing:** Read the current live-preview block. If glow-preview already introduced outer/`MultiEffect`/`*PreviewContent`, only patch inside those groups (Steps below show the full desired tree; drop the MultiEffect section if absent, keep ids).

- [ ] **Step 1: Freehand Live Preview**

Replace freehand block (`main.qml:293-311`) with:

```qml
    // Freehand Live Preview
    Item {
        anchors.fill: parent
        visible: isDrawing && activeDrawTool === "freehand"
        opacity: controller.defaultOpacity

        // MultiEffect lives here only if glow-preview already landed — do not add in this plan.

        Item {
            id: freehandPreviewContent
            anchors.fill: parent
            // If glow present: visible: controller.defaultGlow === 0

            Shape {
                anchors.fill: parent
                visible: controller.defaultRoughness === 0

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

            RoughStroke {
                anchors.fill: parent
                strokes: (controller.defaultRoughness > 0 && activePoints.length >= 2)
                    ? RoughPathGenerator.getSketchyFreehand(
                        activePoints, controller.defaultRoughness, previewSeed)
                    : []
                strokeColor: controller.defaultColor
                strokeWidth: controller.defaultStrokeWidth
                strokeOpacity: 1.0
            }
        }
    }
```

Keep `id: freehandPreviewPath` — `onPositionChanged` assigns it.

- [ ] **Step 2: Guard freehand path updates**

In `main.qml:265-270`:

```qml
            if (activeDrawTool === "freehand") {
                let pts = activePoints;
                pts.push(Qt.point(mouse.x, mouse.y));
                activePoints = pts;
                if (controller.defaultRoughness === 0)
                    freehandPreviewPath.path = activePoints;
            } else {
```

Rough path rebinds from `activePoints` automatically.

- [ ] **Step 3: Rectangle Live Preview**

Replace rectangle block (`main.qml:313-328`). Geometry on outer; local coords for rough generator:

```qml
    // Rectangle Live Preview
    Item {
        visible: isDrawing && activeDrawTool === "rectangle"
        x: previewX
        y: previewY
        width: previewW
        height: previewH
        opacity: controller.defaultOpacity

        // MultiEffect here only if glow-preview already landed.

        Item {
            id: rectPreviewContent
            anchors.fill: parent
            // If glow present: visible: controller.defaultGlow === 0

            Rectangle {
                anchors.fill: parent
                border.color: controller.defaultColor
                border.width: controller.defaultRoughness === 0 ? controller.defaultStrokeWidth : 0
                color: {
                    let c = Qt.color(controller.defaultColor);
                    return Qt.rgba(c.r, c.g, c.b, 0.12);
                }
                radius: controller.defaultBorderRadius
            }

            RoughStroke {
                anchors.fill: parent
                strokes: (controller.defaultRoughness > 0 && width > 1 && height > 1)
                    ? RoughPathGenerator.getSketchyRectangle(
                        0, 0, width, height,
                        controller.defaultRoughness, previewSeed,
                        controller.defaultBorderRadius)
                    : []
                strokeColor: controller.defaultColor
                strokeWidth: controller.defaultStrokeWidth
                strokeOpacity: 1.0
            }
        }
    }
```

- [ ] **Step 4: Ellipse Live Preview**

Replace ellipse block (`main.qml:330-365`). Full-window outer; absolute generator coords:

```qml
    // Ellipse Live Preview
    Item {
        anchors.fill: parent
        visible: isDrawing && activeDrawTool === "ellipse"
        opacity: controller.defaultOpacity

        Item {
            id: ellipsePreviewContent
            anchors.fill: parent

            Shape {
                anchors.fill: parent

                ShapePath {
                    strokeColor: controller.defaultRoughness === 0 ? controller.defaultColor : "transparent"
                    strokeWidth: controller.defaultStrokeWidth
                    fillColor: {
                        let c = Qt.color(controller.defaultColor);
                        return Qt.rgba(c.r, c.g, c.b, 0.12);
                    }

                    startX: previewX + previewW / 2
                    startY: previewY

                    PathArc {
                        x: previewX + previewW / 2
                        y: previewY + previewH
                        radiusX: previewW / 2
                        radiusY: previewH / 2
                        useLargeArc: false
                        direction: PathArc.Clockwise
                    }

                    PathArc {
                        x: previewX + previewW / 2
                        y: previewY
                        radiusX: previewW / 2
                        radiusY: previewH / 2
                        useLargeArc: false
                        direction: PathArc.Clockwise
                    }
                }
            }

            RoughStroke {
                anchors.fill: parent
                strokes: (controller.defaultRoughness > 0 && previewW > 2 && previewH > 2)
                    ? RoughPathGenerator.getSketchyEllipse(
                        previewX, previewY, previewW, previewH,
                        controller.defaultRoughness, previewSeed)
                    : []
                strokeColor: controller.defaultColor
                strokeWidth: controller.defaultStrokeWidth
                strokeOpacity: 1.0
            }
        }
    }
```

- [ ] **Step 5: Line Live Preview**

Replace line block (`main.qml:367-385`):

```qml
    // Line Live Preview
    Item {
        anchors.fill: parent
        visible: isDrawing && activeDrawTool === "line"
        opacity: controller.defaultOpacity

        Item {
            id: linePreviewContent
            anchors.fill: parent

            Shape {
                anchors.fill: parent
                visible: controller.defaultRoughness === 0

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

            RoughStroke {
                anchors.fill: parent
                strokes: (controller.defaultRoughness > 0
                          && (Math.abs(previewW) > 2 || Math.abs(previewH) > 2))
                    ? RoughPathGenerator.getSketchyLine(
                        drawStartPoint.x, drawStartPoint.y,
                        drawStartPoint.x + previewW, drawStartPoint.y + previewH,
                        controller.defaultRoughness, previewSeed)
                    : []
                strokeColor: controller.defaultColor
                strokeWidth: controller.defaultStrokeWidth
                strokeOpacity: 1.0
            }
        }
    }
```

- [ ] **Step 6: Arrow Live Preview**

Replace arrow block (`main.qml:387-434`). Trig props on outer (or keep on a dedicated id); clean + rough both inside `arrowPreviewContent`:

```qml
    // Arrow Live Preview
    Item {
        id: arrowPreview
        anchors.fill: parent
        visible: isDrawing && activeDrawTool === "arrow"
        opacity: controller.defaultOpacity

        property real angle: Math.atan2(previewH, previewW)
        property real len: 10 + controller.defaultStrokeWidth * 1.5
        property real halfAng: Math.PI / 6
        property real tx: drawStartPoint.x + previewW
        property real ty: drawStartPoint.y + previewH
        property real lineLength: Math.sqrt(Math.pow(previewW, 2) + Math.pow(previewH, 2))
        property real stemLength: Math.max(0, lineLength - len * Math.cos(halfAng))
        property real baseX: drawStartPoint.x + stemLength * Math.cos(angle)
        property real baseY: drawStartPoint.y + stemLength * Math.sin(angle)

        Item {
            id: arrowPreviewContent
            anchors.fill: parent

            Shape {
                anchors.fill: parent
                visible: controller.defaultRoughness === 0
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: controller.defaultColor
                    strokeWidth: controller.defaultStrokeWidth
                    fillColor: "transparent"

                    startX: drawStartPoint.x
                    startY: drawStartPoint.y
                    PathLine { x: arrowPreview.baseX; y: arrowPreview.baseY }
                }

                ShapePath {
                    strokeColor: "transparent"
                    fillColor: controller.defaultColor

                    startX: arrowPreview.tx
                    startY: arrowPreview.ty
                    PathLine {
                        x: arrowPreview.tx - arrowPreview.len * Math.cos(arrowPreview.angle - arrowPreview.halfAng)
                        y: arrowPreview.ty - arrowPreview.len * Math.sin(arrowPreview.angle - arrowPreview.halfAng)
                    }
                    PathLine {
                        x: arrowPreview.tx - arrowPreview.len * Math.cos(arrowPreview.angle + arrowPreview.halfAng)
                        y: arrowPreview.ty - arrowPreview.len * Math.sin(arrowPreview.angle + arrowPreview.halfAng)
                    }
                    PathLine { x: arrowPreview.tx; y: arrowPreview.ty }
                }
            }

            RoughStroke {
                anchors.fill: parent
                strokes: (controller.defaultRoughness > 0
                          && (Math.abs(previewW) > 2 || Math.abs(previewH) > 2))
                    ? RoughPathGenerator.getSketchyArrow(
                        drawStartPoint.x, drawStartPoint.y,
                        arrowPreview.tx, arrowPreview.ty,
                        controller.defaultRoughness, previewSeed, arrowPreview.len)
                    : []
                strokeColor: controller.defaultColor
                strokeWidth: controller.defaultStrokeWidth
                strokeOpacity: 1.0
            }
        }
    }
```

- [ ] **Step 7: Performance note**

`getSketchy*` re-runs on binding updates while dragging — same as finalized shapes on geometry change. Accept first.

Only if freehand drag janks: throttle recompute (Nth point or 16ms `Timer` + `property var previewRoughStrokes`). Do **not** throttle in the first pass.

- [ ] **Step 8: Build overlay**

```bash
cmake --build build --target scribbleway-overlay
```

Expected: success; `RoughStroke` type resolves.

- [ ] **Step 9: Commit**

```bash
git add src/overlay/qml/main.qml
git commit -m "feat: rough live previews matching finalized shape rendering"
```

---

### Task 3: Manual verification & regression

**Files:** none (manual)

**Interfaces:**
- Consumes: Tasks 1–2
- Produces: verified acceptance criteria

- [ ] **Step 1: Unit tests (expect no failures)**

```bash
cmake --build build --target shapesmodeltest && ctest --test-dir build -R shapesmodel -V
```

Expected: PASS (no C++ contract changes).

- [ ] **Step 2: Manual overlay checklist**

Launch overlay (project-typical binary from build tree). For each tool with `defaultRoughness` `1` and `2`:

| # | Action | Expected |
|---|--------|----------|
| 1 | Draw rectangle | Sketchy outline during drag; 12% fill; no re-jitter on release |
| 2 | Draw ellipse | Rough stroke during drag; fill stays; stable on commit |
| 3 | Draw line | Sketchy stroke; matches finalized `LineShape` |
| 4 | Draw arrow | Sketchy stem + open head (not filled triangle); matches `ArrowShape` rough mode |
| 5 | Draw freehand | Sketchy polyline during drag; no jump on release |
| 6 | `defaultRoughness === 0`, redraw all | Clean primitives only (no RoughStroke geometry) |
| 7 | Change roughness mid-session | Next drag uses new default immediately |
| 8 | Abort mid-drag (`abortShape` via tool/mode switch) | Preview gone; no shape; next press new seed |
| 9 | If glow-preview also landed: rough + `defaultGlow > 0` | Glow wraps rough strokes (MultiEffect sources `*PreviewContent` containing RoughStroke) |

Optional seed check: `console.log(previewSeed)` in `onPressed` and `finalizeShape` — same value per gesture.

- [ ] **Step 3: Fixup commit if needed**

```bash
git add src/overlay/qml/main.qml
git commit -m "fix: live preview roughness edge cases"
```

(Skip empty commit.)

---

## Test Plan Summary

| Layer | What | How |
|-------|------|-----|
| Unit | C++ / RoughPathGenerator unchanged | `ctest -R shapesmodel` |
| Visual | Rough vs clean per tool | Manual table |
| Continuity | `previewSeed` === finalized `seed` | log or no-jump on release |
| Regression | `defaultRoughness === 0` | Clean path only |
| Composition | RoughStroke inside `*PreviewContent` | Code review + glow+rough manual row |

No new automated QML tests (no overlay gesture harness).

---

## Excalidraw Compatibility Impact

**None.** Preview is ephemeral. Finalized shapes already store `roughness` + `seed`; only the seed moment moves from release → press. `OverlayController::convertToExcalidraw` / `convertFromExcalidraw` untouched.

---

## Acceptance Criteria

1. With `defaultRoughness > 0`, freehand / rectangle / ellipse / line / arrow previews render via `RoughPathGenerator` + `RoughStroke`.
2. Rectangle and ellipse keep translucent fill during rough preview; only the stroke is rough.
3. With `defaultRoughness === 0`, previews match previous clean Qt primitives.
4. On release, committed shape uses the same seed as the preview — no visible re-jitter.
5. Arrow rough preview uses `getSketchyArrow` (open strokes), consistent with `ArrowShape.qml`.
6. `RoughStroke` is nested inside each `*PreviewContent` group (glow MultiEffect source), not a peer of the outer wrapper.
7. Rectangle rough path uses **local** `(0,0,width,height)` when geometry lives on the outer Item.
8. No C++ / D-Bus / applet changes; existing tests pass.
9. `abortShape` still clears preview without writing a shape.

---

## Self-Review

| Spec item | Task |
|-----------|------|
| Apply rough renderer to previews | Task 2 |
| Freehand / rect / ellipse / line / arrow | Task 2 steps 1–6 |
| Avoid jump on release (stable seed) | Task 1 |
| Match finalized dual-path | Task 2 |
| roughness 0 unchanged | Task 2 visibility / border guards |
| Glow composition nesting | Global Constraints + Task 2 structure |
| Excalidraw impact | none |
| Test plan | Task 3 |

No placeholders. No new QVariantMap keys, ShapeRoles, Q_PROPERTYs, or D-Bus signals.
