# Tray Popup: Replace `ColorDialog` with an Inline Side Panel — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The two `ColorDialog`s in `FullRepresentation.qml` open an xdg-desktop-portal dialog. A portal dialog cannot be positioned, ignores the layer-shell parent, and lands fullscreen on the *primary* output — not next to the popup on the *active* one. Replace both with a fixed-width color picker rendered inside the tray popup's own window, sitting immediately to the left of the menu.

**Architecture:** The tray popup is a layer-shell surface anchored `Bottom|Right` (`src/overlay/main.cpp:154`), so its right edge is pinned and **growing its width extends it leftwards**. `TrayPopup.qml` becomes a `RowLayout` of `[ColorPickerPanel][FullRepresentation]`, and the window's `width` grows by the panel's width when the panel is open. One surface, one screen, no second focus source, no portal.

**Tech Stack:** QML (QtQuick, QtQuick.Controls, QtQuick.Layouts) + a small C++ touch-up in `main.cpp`. No new dependencies. `import QtQuick.Dialogs` disappears entirely.

## Prerequisites (already landed, do not redo)

Issues #2 and #3 from the same triage are fixed on this branch:

- `OverlayController::setTrayPopupOpen()` — the overlay yields its exclusive keyboard grab while the popup is open (`overlaycontroller.cpp`), and `main.qml:231` no longer calls `requestActivate()` in that state. **This is what makes an in-window picker with a focusable hex field possible at all.**
- All four `ComboBox`es use `popup.popupType = Controls.Popup.Item`, so no control in the popup spawns a second Wayland surface.
- `fullRoot.pick(selected, fallback)` routes every property *read* to the default when nothing is selected, mirroring the existing setters. **The picker must use `pick()` for its initial color, not `backend.selectedColor` directly.**

## Global Constraints

- **No second window and no portal.** The whole point is one surface on the active output. Do not reach for `Window`, `ApplicationWindow`, `Popup.Window`, `Popup.Native`, or anything from `QtQuick.Dialogs`.
- **The picker must apply live**, like every slider in the menu — call `fullRoot.setColor()` / `setFillColor()` on change. No OK/Cancel round trip. (The popup no longer dismisses on interaction, so live apply is now safe and consistent.)
- Panel width is fixed (208px suggested). The menu's own width must not change when the panel opens or closes.
- Do not regress the `"transparent"` fill swatch — it is a separate control in the fill row and stays as is.
- `./dev-reinstall.sh` must keep producing a working installable build; verify on the live KDE/Wayland session, not just `ctest`.

---

## File Structure

**Create:**
- `src/overlay/qml/ColorPickerPanel.qml` — self-contained SV square + hue slider + hex field.

**Modify:**
- `src/overlay/qml/TrayPopup.qml` — `anchors.fill` child → `RowLayout`; dynamic window width.
- `src/overlay/qml/FullRepresentation.qml` — delete both `ColorDialog`s and the `QtQuick.Dialogs` import; the two "Custom" buttons emit a signal instead.
- `src/overlay/main.cpp` — recompute the popup input-region exclusion when the window's width changes.
- `src/overlay/CMakeLists.txt` — register the new QML file.

---

### Task 1: `ColorPickerPanel.qml` — the picker component

**Files:** create `src/overlay/qml/ColorPickerPanel.qml`; add it to `QML_FILES` in `src/overlay/CMakeLists.txt`.

**Interface (fixed — Task 2 codes against exactly this):**

```qml
Item {
    property color selectedColor      // two-way: set to seed the picker, read on change
    signal closeRequested()
    implicitWidth: 208
}
```

**Steps:**

- [ ] Lay out top-to-bottom: SV square (square, panel-width), hue slider, hex `TextField`, current-color preview swatch, and a close (`×`) button in a header row.
- [ ] **SV square without a Canvas:** a `Rectangle` filled with `Qt.hsva(hue, 1, 1, 1)`, overlaid by a horizontal `Gradient` white→transparent (saturation), overlaid by a vertical `Gradient` transparent→black (value). Use `Gradient { orientation: Gradient.Horizontal }` — two stacked `Rectangle`s, no `Canvas`, no shader.
- [ ] A `MouseArea` over the square (`onPressed` + `onPositionChanged`, so dragging works) maps `x/width` → saturation and `y/height` → value. Draw a small ring/crosshair at the current position.
- [ ] Hue slider: `Controls.Slider` from 0 to 1 with a custom `background` `Rectangle` carrying a 7-stop gradient (red → yellow → green → cyan → blue → magenta → red).
- [ ] Recompose on any change: `selectedColor = Qt.hsva(hue, sat, val, 1)`.
- [ ] **Seeding from the outside:** when `selectedColor` is assigned externally, decompose it via the built-in `color` members `hsvHue`, `hsvSaturation`, `hsvValue` — do not hand-roll RGB→HSV. Guard against a feedback loop between "user moved the square" and "property was assigned back": a `property bool updating` latch, or only re-seed while the panel is not being dragged. **This loop is the one real trap in this task — get it right before moving on.**
- [ ] Hex `TextField`: shows `selectedColor.toString()` (`#rrggbb`), accepts typed input, `validator: RegularExpressionValidator { regularExpression: /^#?[0-9A-Fa-f]{6}$/ }`, commits on `accepted`. `hsvHue` is `-1` for achromatic colors — clamp to the previous hue rather than snapping to red.
- [ ] Match the existing look: reuse the `theme` idiom from `FullRepresentation.qml` (`gridUnit: 18`, `smallSpacing: 4`, `palette.*`) rather than inventing new constants.

**Verification:** `qmllint` clean of *new* warnings (the file already emits pre-existing `layout-positioning` and `unqualified` warnings elsewhere — do not add more). Then a throwaway harness: a `Window` containing just `ColorPickerPanel` plus a `Text` bound to `selectedColor`, run with `qml6`. Confirm: dragging the square updates the hex; typing a hex moves the crosshair *and* the hue slider; the round trip `#e63946` → drag → type `#e63946` returns to the same crosshair position. Delete the harness before finishing.

---

### Task 2: Wire the panel into the popup and delete both `ColorDialog`s

**Files:** `src/overlay/qml/TrayPopup.qml`, `src/overlay/qml/FullRepresentation.qml`. **Depends on Task 1.**

**Steps:**

- [ ] In `FullRepresentation.qml`: delete the `colorDialog` and `fillColorDialog` `ColorDialog` blocks and the `import QtQuick.Dialogs` line.
- [ ] Add to `fullRoot`: `signal customColorRequested(string current, bool isFill)`. The stroke "Custom" button emits `customColorRequested(fullRoot.pick(backend.selectedColor, backend.defaultColor), false)`; the fill one emits `(pick(backend.selectedFillColor, backend.defaultFillColor), true)`. **Use `pick()`** — with nothing selected, `selectedColor` alone is stale (see Prerequisites).
- [ ] Also expose `function applyCustomColor(c, isFill)` on `fullRoot` that forwards to the existing `setColor` / `setFillColor`, so `TrayPopup.qml` never touches `backend` directly.
- [ ] In `TrayPopup.qml`: replace the single `anchors.fill` child with a `RowLayout` — `ColorPickerPanel` first (left), `FullRepresentation` second, `Layout.fillWidth: true`.
- [ ] Window sizing: keep the menu at its current 432 and make the window `width: 432 + (picker.visible ? picker.implicitWidth + spacing : 0)`. Raise/relax `minimumWidth` (currently 400) so it never fights the grown width.
- [ ] Panel opens on `customColorRequested` (remembering `isFill`), closes on `closeRequested`, and on change calls `applyCustomColor(picker.selectedColor, isFillMode)`.
- [ ] Opening the panel a second time for the *other* target must re-seed from that target's current color.

**Risk — read before starting:** it is unverified whether Qt/LayerShellQt re-commits an already-mapped layer surface when `width` changes. Check this **first**, with a two-line hack (a timer that bumps `width` after show), before building the rest of the task. If it does not resize live, the fallback is: keep the window permanently at the full grown width, set the `Window`'s `color: "transparent"`, give `FullRepresentation` its own opaque background `Rectangle`, and mask the unused strip out of the surface's input region via `QQuickWindow::setMask()` from `main.cpp` so the invisible area is neither visible nor clickable. Report which path you took — it changes Task 3.

**Verification:** run `./build/bin/scribbleway-overlay` on the live session. Click the tray icon → popup appears on the *active* screen. Click "Custom" → panel appears **to the left**, same screen, popup does **not** dismiss, menu width unchanged. Drag the SV square with no shape selected → the "Color:" row's preselected swatch highlight follows. Select a shape, open the fill picker → the shape's fill updates live. Close the panel → the window shrinks back.

---

### Task 3: Keep the overlay's input-region exclusion in sync with the popup's width

**Files:** `src/overlay/main.cpp`. **Independent of Tasks 1 and 2 — can run in parallel.**

**Problem:** `showPopup()` (`main.cpp:181`) computes the rect carved out of the overlay's input region once, from `popupWindow->width()`, so the popup stays clickable while a drawing tool is grabbing input. When Task 2 grows the window, that rect goes stale and the new left-hand strip is unclickable whenever a tool is active — a bug that only shows up with a tool active, which is exactly how it will be missed.

**Steps:**

- [ ] Extract the exclusion-rect calculation out of `showPopup()` into its own `updatePopupExclusion()` lambda.
- [ ] Call it from `showPopup()` as today, **and** connect it to `QWindow::widthChanged` on `popupWindow` in the `objectCreated` handler (`main.cpp:202`).
- [ ] Leave the `controller.setPopupExclusion(QRect())` reset in `hidePopup` alone.
- [ ] If Task 2 reports it took the fixed-width fallback, this task instead owns the `setMask()` call — coordinate before writing code.

**Verification:** activate a drawing tool, open the popup, open the color panel, and confirm a click in the panel's left column reaches the panel and does not draw on the overlay.

---

## Out of Scope

- An alpha slider. The app already has separate Opacity and Fill Opacity sliders; a third way to say the same thing is a bug factory. (Explicitly declined during triage.)
- Recently-used / saved custom colors.
- Making the 6 preset swatches configurable.
