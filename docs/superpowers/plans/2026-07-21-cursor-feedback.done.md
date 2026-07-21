# Cursor Feedback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give Scribbleway clear visual cursor feedback for the active tool and mode so users always know whether they are drawing, selecting, or editing text.

**Architecture:** Keep feedback entirely in the overlay QML layer. Bind `drawMouseArea.cursorShape` (and select/background/drag cursors) to mode + tool. Add a lightweight cursor-following tool badge (pure QML geometry + label) that tracks `lastMousePos` while a draw tool is active. No C++/D-Bus/model changes; no Excalidraw impact.

**Tech Stack:** Qt 6 QML (`QtQuick`, `QtQuick.Controls`), existing `OverlayController` properties (`activeTool`, `currentMode`), existing `hoverTracker` / `lastMousePos` in `src/overlay/qml/main.qml`.

## Global Constraints

* **No new dependencies** — do not add Kirigami, icon theme loaders, or bundled PNG/SVG cursor packs unless stock Qt cursors prove insufficient; prefer pure QML badge + `Qt.CursorShape`.
* **Wayland-safe** — cursor position must continue to come from overlay `MouseArea` hover events (`hoverTracker` / `drawMouseArea`), never assume global `QCursor::pos()` alone.
* **Passthrough mode** — no badge, no forced overlay cursor when `controller.currentMode === "passthrough"` and no draw tool.
* **Do not steal shape interaction** — badge must use `enabled: false` / no mouse acceptance; shape resize/drag cursors in `BaseShape.qml` still win when those `MouseArea`s are under the pointer.
* **YAGNI** — no permanent docked toolbar HUD on the overlay in this plan (covers annotations, multi-monitor pain). Rejected alternative documented below.
* **Excalidraw** — no serialization, shape keys, roles, or clipboard changes.

---

## File Structure & Decomposition

| File | Responsibility |
|------|----------------|
| `src/overlay/qml/main.qml` | Tool→cursor mapping helpers; bind `drawMouseArea.cursorShape`; select-background cursor; floating `toolCursorBadge`; keep `lastMousePos` updated from draw hover too |
| `src/overlay/qml/ToolCursorBadge.qml` | Self-contained badge: tool glyph + optional shortcut letter; pure QML, no mouse input |
| `src/overlay/qml/shapes/BaseShape.qml` | Open/closed hand cursor on shape drag body (select mode) |
| `src/overlay/CMakeLists.txt` | Register `qml/ToolCursorBadge.qml` in `qt_add_qml_module` |
| Manual smoke only | Cursor feedback is visual/QML; no model contract to unit-test |

### Rejected alternative: docked toolbar HUD

A fixed top-left tool strip on the overlay would work, but:
1. Covers whatever the user is annotating.
2. Needs input-region holes / click handling in select+draw.
3. Duplicates the Plasma applet tool picker.

Cursor shapes + following badge give the same “which tool is active?” answer without those costs.

### Design summary

```
Mode/tool                  System cursor              Badge
-------------------------  -------------------------  ----------------------
passthrough                (desktop default)          hidden
select (empty bg)          Qt.ArrowCursor             hidden
select (over shape body)   OpenHand / ClosedHand      hidden
select (resize handles)    existing Size* cursors     hidden (unchanged)
draw freehand              Qt.CrossCursor             pencil glyph + "F"
draw rectangle             Qt.CrossCursor             rect glyph + "R"
draw ellipse               Qt.CrossCursor             ellipse glyph + "E"
draw line                  Qt.CrossCursor             line glyph + "L"
draw arrow                 Qt.CrossCursor             arrow glyph + "A"
draw text                  Qt.IBeamCursor             "T" glyph + "T"
```

Stock Qt has no distinct rect/ellipse/line cursors, so the badge carries tool identity; system cursor still communicates “drawing vs text vs select.”

---

### Task 1: Extract ToolCursorBadge component

**Files:**
- Create: `src/overlay/qml/ToolCursorBadge.qml`
- Modify: `src/overlay/CMakeLists.txt`

**Interfaces:**
- Consumes: `tool` (`string`: `freehand|rectangle|ellipse|line|arrow|text`), optional `accent` (`color`, default stroke color)
- Produces: visual-only `Item` 28×28 with rounded chip + glyph; `enabled: false`, no mouse

- [ ] **Step 1: Create `ToolCursorBadge.qml`**

```qml
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
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = root.accent;
                ctx.lineWidth = 1.8;
                ctx.lineCap = "round";
                ctx.beginPath();
                ctx.moveTo(2, 12);
                ctx.quadraticCurveTo(5, 2, 8, 8);
                ctx.quadraticCurveTo(11, 14, 14, 4);
                ctx.stroke();
            }
            onVisibleChanged: if (visible) requestPaint()
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
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = root.accent;
                    ctx.lineWidth = 1.6;
                    ctx.lineCap = "round";
                    ctx.lineJoin = "round";
                    ctx.beginPath();
                    ctx.moveTo(9, 3);
                    ctx.lineTo(14, 4);
                    ctx.lineTo(11, 9);
                    ctx.stroke();
                }
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
```

- [ ] **Step 2: Register the QML file in CMake**

In `src/overlay/CMakeLists.txt`, add to `QML_FILES` (after `qml/main.qml`):

```cmake
        qml/main.qml
        qml/ToolCursorBadge.qml
        qml/shapes/BaseShape.qml
```

- [ ] **Step 3: Build to verify the module picks it up**

```bash
cmake --build build -j"$(nproc)" --target scribbleway-overlay
```

Expected: build succeeds; no missing QML module errors.

- [ ] **Step 4: Commit**

```bash
git add src/overlay/qml/ToolCursorBadge.qml src/overlay/CMakeLists.txt
git commit -m "feat(overlay): add ToolCursorBadge QML component"
```

---

### Task 2: Per-tool / per-mode cursor shapes in main.qml

**Files:**
- Modify: `src/overlay/qml/main.qml` (properties ~14–45, `hoverTracker` ~203–212, `drawMouseArea` ~241–289, background `MouseArea` ~706–746)

**Interfaces:**
- Consumes: `activeDrawTool`, `controller.currentMode`, `textEditor.visible`
- Produces: `canvasWindow.cursorForTool(tool)` helper; bound cursors on draw + background mouse areas; draw hover updates `lastMousePos`

- [ ] **Step 1: Add cursor helper properties near the top of `canvasWindow`**

After `property real selectH: 0` (~line 45), add:

```qml
    // Cursor feedback -------------------------------------------------
    // Maps draw tools to Qt.CursorShape. Most geometry tools share
    // CrossCursor; the floating ToolCursorBadge disambiguates them.
    function cursorForDrawTool(tool) {
        switch (tool) {
        case "text":
            return Qt.IBeamCursor;
        case "freehand":
        case "rectangle":
        case "ellipse":
        case "line":
        case "arrow":
            return Qt.CrossCursor;
        default:
            return Qt.ArrowCursor;
        }
    }

    readonly property bool showToolCursorBadge:
        activeDrawTool !== ""
        && controller.currentMode !== "passthrough"
        && !textEditor.visible
```

- [ ] **Step 2: Make `hoverTracker` keep working under draw mode**

`drawMouseArea` sits above `hoverTracker` and currently does not set `hoverEnabled`, so `lastMousePos` only updates while pressing. Fix both areas.

Replace `hoverTracker` block (~203–212) with:

```qml
    // Mouse hover tracker to capture cursor position under Wayland
    MouseArea {
        id: hoverTracker
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        // Only useful when drawMouseArea is disabled; when drawing,
        // drawMouseArea owns hover (see below).
        enabled: !(activeDrawTool !== "" && !textEditor.visible)
        onPositionChanged: (mouse) => {
            canvasWindow.lastMousePos = Qt.point(mouse.x, mouse.y)
        }
    }
```

- [ ] **Step 3: Bind `drawMouseArea` cursor + hover tracking**

Replace the `drawMouseArea` header (~242–247) so it becomes:

```qml
    MouseArea {
        id: drawMouseArea
        anchors.fill: parent
        enabled: activeDrawTool !== "" && !textEditor.visible
        hoverEnabled: true
        cursorShape: canvasWindow.cursorForDrawTool(activeDrawTool)

        onPositionChanged: (mouse) => {
            canvasWindow.lastMousePos = Qt.point(mouse.x, mouse.y)
            if (!isDrawing) return
            // ... existing drawing preview logic stays below ...
```

Important: merge with the existing `onPositionChanged` body (freehand point batching / preview sizing). Do **not** create two `onPositionChanged` handlers. Final shape:

```qml
        onPositionChanged: (mouse) => {
            canvasWindow.lastMousePos = Qt.point(mouse.x, mouse.y);

            if (!isDrawing) return;

            if (activeDrawTool === "freehand") {
                let pts = activePoints;
                pts.push(Qt.point(mouse.x, mouse.y));
                activePoints = pts;
                freehandPreviewPath.path = activePoints;
            } else {
                if (activeDrawTool === "line" || activeDrawTool === "arrow") {
                    previewW = mouse.x - drawStartPoint.x;
                    previewH = mouse.y - drawStartPoint.y;
                } else {
                    previewX = Math.min(mouse.x, drawStartPoint.x);
                    previewY = Math.min(mouse.y, drawStartPoint.y);
                    previewW = Math.abs(mouse.x - drawStartPoint.x);
                    previewH = Math.abs(mouse.y - drawStartPoint.y);
                }
            }
        }
```

Also update `onPressed` to seed `lastMousePos`:

```qml
        onPressed: (mouse) => {
            canvasWindow.lastMousePos = Qt.point(mouse.x, mouse.y);
            isDrawing = true;
            // ... rest unchanged ...
```

- [ ] **Step 4: Cursor on select-mode background MouseArea**

In the background interaction `MouseArea` (~706–710), add `hoverEnabled` + `cursorShape`:

```qml
    MouseArea {
        anchors.fill: parent
        z: -1 // Behind shapes and draw capture layers
        enabled: controller.currentMode === "select" || controller.selectedIndex !== -1 || textEditor.visible
        hoverEnabled: true
        cursorShape: textEditor.visible ? Qt.ArrowCursor
                     : (isSelectingFrame ? Qt.CrossCursor : Qt.ArrowCursor)

        onPositionChanged: (mouse) => {
            canvasWindow.lastMousePos = Qt.point(mouse.x, mouse.y);
            if (isSelectingFrame) {
                selectX = Math.min(mouse.x, selectStartPoint.x);
                selectY = Math.min(mouse.y, selectStartPoint.y);
                selectW = Math.abs(mouse.x - selectStartPoint.x);
                selectH = Math.abs(mouse.y - selectStartPoint.y);
                controller.selectShapesInRect(selectX, selectY, selectW, selectH, mouse.modifiers & Qt.ShiftModifier);
            }
        }
```

Merge with the existing `onPositionChanged` (do not duplicate the handler). Keep `onPressed` / `onReleased` as they are.

- [ ] **Step 5: Rebuild and smoke-check cursor shapes**

```bash
cmake --build build -j"$(nproc)" --target scribbleway-overlay
# run overlay (project-typical):
# build/bin/scribbleway-overlay   # or installed path
```

Manual:
1. Enter draw freehand → cross cursor.
2. Press `T` → I-beam cursor.
3. Press `X` select mode on empty canvas → arrow cursor.
4. Drag selection marquee → cross cursor while dragging.

- [ ] **Step 6: Commit**

```bash
git add src/overlay/qml/main.qml
git commit -m "feat(overlay): bind per-tool and select-mode cursor shapes"
```

---

### Task 3: Floating tool badge following the cursor

**Files:**
- Modify: `src/overlay/qml/main.qml` (insert badge near live-preview / after `drawMouseArea`, before or after selection frame UI ~291–447)

**Interfaces:**
- Consumes: `showToolCursorBadge`, `activeDrawTool`, `lastMousePos`, `controller.defaultColor`
- Produces: on-canvas badge offset southeast of hotspot so it does not cover the draw point

- [ ] **Step 1: Instantiate `ToolCursorBadge` in `main.qml`**

Place **after** live preview shapes and **above** normal content z-order so it stays visible while drawing. Suggested insertion point: immediately after the Arrow Live Preview `Shape` block ends (~line 434), before `// --- SELECTION FRAME UI ---`:

```qml
    // --- TOOL CURSOR BADGE (follows pointer while a draw tool is active) ---
    ToolCursorBadge {
        id: toolCursorBadge
        visible: canvasWindow.showToolCursorBadge
        tool: canvasWindow.activeDrawTool
        accent: controller.defaultColor
        // Offset so the badge sits SE of the hotspot and does not hide the tip
        x: canvasWindow.lastMousePos.x + 16
        y: canvasWindow.lastMousePos.y + 16
        z: 1000
        opacity: canvasWindow.isDrawing ? 0.55 : 0.92

        // Keep on-screen near edges
        Binding on x {
            when: toolCursorBadge.visible
            value: Math.min(canvasWindow.width - toolCursorBadge.width - 4,
                            Math.max(4, canvasWindow.lastMousePos.x + 16))
        }
        Binding on y {
            when: toolCursorBadge.visible
            value: Math.min(canvasWindow.height - toolCursorBadge.height - 4,
                            Math.max(4, canvasWindow.lastMousePos.y + 16))
        }
    }
```

Notes:
- Prefer the `Binding on x/y` form **or** plain `x:`/`y:` expressions with `Math.min/max` — not both fighting. Cleaner single form:

```qml
    ToolCursorBadge {
        id: toolCursorBadge
        visible: canvasWindow.showToolCursorBadge
        tool: canvasWindow.activeDrawTool
        accent: controller.defaultColor
        z: 1000
        opacity: canvasWindow.isDrawing ? 0.55 : 0.92
        x: {
            let nx = canvasWindow.lastMousePos.x + 16;
            return Math.min(canvasWindow.width - width - 4, Math.max(4, nx));
        }
        y: {
            let ny = canvasWindow.lastMousePos.y + 16;
            return Math.min(canvasWindow.height - height - 4, Math.max(4, ny));
        }
    }
```

- [ ] **Step 2: Rebuild and smoke-test badge**

```bash
cmake --build build -j"$(nproc)" --target scribbleway-overlay
```

Manual:
1. Activate freehand → badge with scribble + `F` follows cursor.
2. Cycle tools with `R`/`E`/`L`/`A`/`T`/`F` → glyph + letter update immediately; text tool also shows I-beam.
3. Change color in applet → badge accent tracks `controller.defaultColor`.
4. Press to draw → badge dims (`opacity` 0.55) so it is less distracting mid-stroke.
5. Enter select mode (`X`) or passthrough → badge hides.
6. Open text editor → badge hides; I-beam/editor focus remains usable.
7. Move cursor into bottom-right corner → badge clamps inside window.

- [ ] **Step 3: Commit**

```bash
git add src/overlay/qml/main.qml
git commit -m "feat(overlay): show floating tool badge at cursor"
```

---

### Task 4: Shape drag hand cursors in BaseShape

**Files:**
- Modify: `src/overlay/qml/shapes/BaseShape.qml` (`dragArea` ~76–134)

**Interfaces:**
- Consumes: `dragArea.pressed`, `isLocked`
- Produces: `OpenHandCursor` when hovering a movable shape body; `ClosedHandCursor` while dragging

- [ ] **Step 1: Set cursor on `dragArea`**

In `BaseShape.qml`, update the main interaction `MouseArea` (~77–84):

```qml
    MouseArea {
        id: dragArea
        x: mode === "line" ? Math.min(shapeFromX, shapeToX) - 10 : shapeX
        y: mode === "line" ? Math.min(shapeFromY, shapeToY) - 10 : shapeY
        width: mode === "line" ? Math.abs(shapeFromX - shapeToX) + 20 : shapeWidth
        height: mode === "line" ? Math.abs(shapeFromY - shapeToY) + 20 : shapeHeight
        enabled: !isLocked
        hoverEnabled: true
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
```

Leave resize-handle and endpoint `cursorShape` blocks unchanged (`SizeFDiagCursor`, `SizeBDiagCursor`, `SizeVerCursor`, `SizeHorCursor`, `SizeAllCursor`).

- [ ] **Step 2: Rebuild and smoke-test**

```bash
cmake --build build -j"$(nproc)" --target scribbleway-overlay
```

Manual (select mode):
1. Hover a selected unlocked shape body → open hand.
2. Press-drag → closed hand.
3. Hover corner handle → diagonal resize cursor still wins (child MouseArea).
4. Locked shape → dragArea disabled; no hand cursor.

- [ ] **Step 3: Commit**

```bash
git add src/overlay/qml/shapes/BaseShape.qml
git commit -m "feat(overlay): open/closed hand cursors on shape drag"
```

---

### Task 5: Final verification matrix

**Files:** none (verification only)

- [ ] **Step 1: Full rebuild**

```bash
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Expected: existing tests still pass (no model/API changes).

- [ ] **Step 2: Run the acceptance matrix**

| # | Action | Expected |
|---|--------|----------|
| 1 | Overlay passthrough | Desktop cursor; no badge |
| 2 | Enter select (`Meta+Shift+X` or applet) | Arrow on empty canvas; no badge |
| 3 | Hover/drag shape | Open/closed hand; resize handles keep Size* cursors |
| 4 | Marquee select on empty bg | Cross while dragging frame |
| 5 | Tool freehand (`F`) | Cross + freehand badge follows pointer |
| 6 | Tool rectangle (`R`) | Cross + rect badge |
| 7 | Tool ellipse (`E`) | Cross + ellipse badge |
| 8 | Tool line (`L`) | Cross + line badge |
| 9 | Tool arrow (`A`) | Cross + arrow badge |
| 10 | Tool text (`T`) | I-beam + text badge; click places text editor; badge hides while editor open |
| 11 | Switch tool mid-hover via shortcut | Cursor + badge update without click |
| 12 | Change default color | Badge accent updates |
| 13 | Draw stroke | Badge remains offset, dims while `isDrawing` |
| 14 | Cursor at screen edges | Badge stays fully inside overlay window |
| 15 | Escape / passthrough | Badge gone; cursors restore |

- [ ] **Step 3: Final commit only if polish fixes were needed**

```bash
git status
# if dirty:
git add -u src/overlay
git commit -m "fix(overlay): polish cursor feedback edge cases"
```

---

## Test Plan

### Automated
* No new unit tests required — behavior is pure QML visual/input chrome with no `QVariantMap` keys, `ShapeRoles`, `Q_PROPERTY`s, or D-Bus signals.
* Regression: full existing suite via `ctest --test-dir build --output-on-failure` must stay green.

### Manual (required)
* Walk the acceptance matrix in Task 5 on a real Plasma/Wayland session (cursor themes and Wayland pointer constraints differ from X11).
* Confirm badge does **not** block drawing: start a freehand stroke with the badge under the path start — stroke must begin at the true hotspot, not the badge corner (`enabled: false` on badge).
* Confirm resize handles still override hand cursor when the pointer is on a handle.

---

## Excalidraw Compatibility Impact

**None.**

* No shape dictionary keys added/removed.
* No changes to `convertToExcalidraw()` / `convertFromExcalidraw()`.
* No clipboard format changes.
* Cursor/badge are ephemeral overlay UI only.

---

## Acceptance Criteria

1. With any draw tool active, the system cursor is **not** a silent default arrow: geometry tools use `Qt.CrossCursor`, text uses `Qt.IBeamCursor`.
2. A floating tool badge follows the pointer while a draw tool is active, shows a distinct glyph per tool, and shows the tool’s shortcut letter (`F/R/E/L/A/T`).
3. Badge hides in select mode, passthrough mode, and while the text editor is open.
4. Badge never captures mouse input and never prevents shape creation or selection.
5. Select-mode shape bodies show open/closed hand cursors; existing resize/endpoint cursors remain correct.
6. `lastMousePos` updates on hover in both select and draw modes (paste-at-cursor and badge tracking keep working under Wayland).
7. No new runtime dependencies; CMake only gains `ToolCursorBadge.qml`.
8. Existing automated tests pass unchanged.
9. No Excalidraw serialization or model schema changes.

---

## Implementation Notes for Agents

* **Single `onPositionChanged` per MouseArea** — QML will not merge duplicate handlers the way you might expect; always edit the existing handler body.
* **Z-order:** `toolCursorBadge.z: 1000` keeps it above shape delegates and live previews; background select area stays `z: -1`.
* **Do not** call `QGuiApplication::setOverrideCursor` from C++ for this feature — window-local `MouseArea.cursorShape` composes correctly with child handles and avoids sticky global override bugs when the overlay hides.
* If a future plan adds custom bitmap cursors, replace `cursorForDrawTool()` only; keep the badge as secondary confirmation.
* Shortcut letters in the badge are **display-only** mirrors of defaults in `OverlayController` local shortcuts (`tool_freehand` → `F`, etc. in `overlaycontroller.cpp` ~40–45). Do not bind live to rebinding unless a later plan asks for it — static letters match the shipped defaults and avoid QML↔map churn.
