# Roughness Slider in Applet Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose roughness (Excalidraw sloppiness levels 0/1/2) as a discrete control in the Plasma applet FullRepresentation, wired through AppletBackend over the existing D-Bus `updateProperties` path.

**Architecture:** Roughness is already a first-class shape property and default on the overlay daemon. This plan only bridges selection state into the applet and adds a UI control. Prefer a **ComboBox** with named levels (Architect / Artist / Cartoonist) over a continuous slider — three discrete values map poorly to a pixel-style slider and match Excalidraw’s labels.

**Tech Stack:** Qt 6, C++20, QML (PlasmaComponents / QtQuick.Controls), D-Bus.

## Motivation

- Hotkey `S` (`cycleRoughness`) is the only way to change roughness today.
- FullRepresentation already has Width, Opacity, Glow, Border Radius, Font — roughness is the missing stroke-style control.
- Users editing from the panel (no overlay keyboard focus) cannot change roughness at all.

## Design

### Levels (unchanged semantics)

| Value | Label        | Meaning                          |
|------:|--------------|----------------------------------|
| 0     | Architect    | Neat / straight strokes          |
| 1     | Artist       | Medium hand-drawn (default)      |
| 2     | Cartoonist   | High sloppiness                  |

### Data flow (existing + new)

```
FullRepresentation ComboBox
  → AppletBackend::setRoughness(int)
    → D-Bus updateProperties({ roughness: N })
      → OverlayController::updateProperties
        → setDefaultRoughness(N)          // always
        → updateShape on selected shapes  // if any
        → notifySelectionChanged()
          → selectionChanged(getSelectionState())  // already includes "roughness"
            → AppletBackend::onSelectionChanged
              → m_selectedRoughness / selectionChanged
                → ComboBox currentIndex binding
```

### What already exists (do not reimplement)

| Piece | Location | Status |
|-------|----------|--------|
| Shape key `roughness` | `QVariantMap` / `ShapesModel::RoughnessRole` | Done |
| `defaultRoughness` Q_PROPERTY | `overlaycontroller.h:43`, impl `.cpp:235-246` | Done |
| `getSelectionState()["roughness"]` | `overlaycontroller.cpp:387,401` | Done |
| `updateProperties` handles `roughness` | `overlaycontroller.cpp:432-434` + shape update loop | Done |
| `cycleRoughness()` / shortcut `S` | `overlaycontroller.cpp:870-889`, `main.qml:659-662` | Done |
| Rendering via `modelRoughness` | `BaseShape.qml` + shape delegates | Done |
| Excalidraw serialize/deserialize | `convertTo/FromExcalidraw` | Done |
| Unit coverage for cycle/default | `tests/shapesmodeltest.cpp::testRoughness` | Done |

### What this plan adds

1. `AppletBackend`: `selectedRoughness` property + `setRoughness(int)` + parse in `onSelectionChanged`.
2. `FullRepresentation.qml`: Roughness ComboBox row (after Glow, before Border Radius).
3. Tests: extend `testAppletBackendIntegration` (and optionally assert default in no-selection path).

### Optional hardening (recommended, small)

- Clamp in `OverlayController::setDefaultRoughness` and/or `AppletBackend::setRoughness` to `qBound(0, roughness, 2)`.
- No new D-Bus signal or method is required; reuse `updateProperties` and `selectionChanged`.

### Explicit non-goals

- Live draw-preview roughness (separate plan if any).
- Changing RoughPathGenerator math or seed behavior.
- Replacing hotkey `S` — keep cycle; applet is additive.
- Per-shape roughness UI beyond selection (multi-select already goes through `updateProperties` selected loop).

## Excalidraw compatibility impact

**None.** Roughness values and clipboard mapping are unchanged. Applet only sets the same `0|1|2` integers already used by `convertToExcalidraw` / `convertFromExcalidraw`.

---

## File Structure

| File | Change |
|------|--------|
| `src/applet-plugin/appletbackend.h` | `selectedRoughness` Q_PROPERTY, getter, `setRoughness`, member |
| `src/applet-plugin/appletbackend.cpp` | getter, setter via D-Bus, parse in `onSelectionChanged` |
| `applet/contents/ui/FullRepresentation.qml` | Roughness ComboBox row |
| `src/overlay/overlaycontroller.cpp` | Optional clamp in `setDefaultRoughness` |
| `tests/shapesmodeltest.cpp` | AppletBackend roughness round-trip assertions |

---

### Task 1: AppletBackend — expose selectedRoughness

**Files:**
- Modify: `src/applet-plugin/appletbackend.h`
- Modify: `src/applet-plugin/appletbackend.cpp`
- Test: `tests/shapesmodeltest.cpp` (Task 3)

**Pattern to mirror:** `selectedGlow` / `setGlow` (header lines 25, 47, 64; cpp 104-107, 155-158, 362).

- [ ] **Step 1: Declare property, API, and member in `appletbackend.h`**

After `selectedGlow` property (~line 25):

```cpp
    Q_PROPERTY(int selectedRoughness READ selectedRoughness NOTIFY selectionChanged)
```

After `int selectedGlow() const;` (~line 47):

```cpp
    int selectedRoughness() const;
```

After `Q_INVOKABLE void setGlow(int glow);` (~line 64):

```cpp
    Q_INVOKABLE void setRoughness(int roughness);
```

After `int m_selectedGlow = 10;` (~line 112):

```cpp
    int m_selectedRoughness = 1;  // Artist — matches OverlayController::m_defaultRoughness
```

- [ ] **Step 2: Implement getter in `appletbackend.cpp`**

After `selectedGlow()` (~line 104-107):

```cpp
int AppletBackend::selectedRoughness() const
{
    return m_selectedRoughness;
}
```

- [ ] **Step 3: Implement `setRoughness` D-Bus forwarder**

After `setGlow` (~line 155-158), same pattern as glow:

```cpp
void AppletBackend::setRoughness(int roughness)
{
    roughness = qBound(0, roughness, 2);
    sendDBus(QStringLiteral("updateProperties"),
             { QVariantMap{{QStringLiteral("roughness"), roughness}} });
}
```

Include `<QtGlobal>` / already available via Qt headers for `qBound`.

- [ ] **Step 4: Parse roughness in `onSelectionChanged`**

In `appletbackend.cpp` `onSelectionChanged` (~lines 349-363), after `m_selectedGlow` assignment:

```cpp
    m_selectedGlow = demarshalled.value(QStringLiteral("glow"), 10).toInt();
    m_selectedRoughness = demarshalled.value(QStringLiteral("roughness"), 1).toInt();
    Q_EMIT selectionChanged();
```

Notes:
- Default fallback `1` matches `OverlayController::m_defaultRoughness` and `getSelectionState()` when no selection.
- Existing `getSelectionState()` already emits `roughness` for both selected and default paths (`overlaycontroller.cpp:387` and `:401`) — no overlay header/signal changes required.
- `selectionChanged` NOTIFY is already on the new Q_PROPERTY; no new signal.

- [ ] **Step 5: Build applet plugin**

```bash
cmake --build build --target scribblewaybackend
```

Expected: clean compile.

- [ ] **Step 6: Commit**

```bash
git add src/applet-plugin/appletbackend.h src/applet-plugin/appletbackend.cpp
git commit -m "feat(applet): expose selectedRoughness and setRoughness over D-Bus"
```

---

### Task 2: FullRepresentation — Roughness ComboBox

**Files:**
- Modify: `applet/contents/ui/FullRepresentation.qml`

**UI placement:** Insert a new row after the Glow slider block (ends ~line 367) and before Border Radius (~line 369). Matches “stroke style” grouping with width/opacity/glow.

**Control choice:** `Controls.ComboBox` (already imported/used for tool and font) with three labels. Index === roughness value (0/1/2). Always visible (applies to all stroke tools and defaults when nothing selected) — same visibility policy as Width/Opacity/Glow, not conditional like Radius/Font.

- [ ] **Step 1: Add Roughness row**

Insert:

```qml
        // Row: Roughness (Excalidraw sloppiness levels)
        RowLayout {
            Layout.fillWidth: true

            PlasmaComponents.Label {
                text: "Roughness:"
                width: Kirigami.Units.gridUnit * 4
            }

            Controls.ComboBox {
                Layout.fillWidth: true
                model: ["Architect", "Artist", "Cartoonist"]
                // selectedRoughness is always synced from getSelectionState
                // (selection or defaults when hasSelection === false)
                currentIndex: Math.max(0, Math.min(2, root.backend.selectedRoughness))
                onActivated: (index) => {
                    root.backend.setRoughness(index)
                }
            }
        }
```

Label width: other rows use `Kirigami.Units.gridUnit * 3`; `"Roughness:"` is longer — `* 4` avoids truncation. If visual alignment with Width/Glow is preferred, keep `* 3` and accept elide, or use a shorter label `"Rough:"`.

**Binding notes:**
- Do **not** hardcode a local default of `1` when `!hasSelection` — `onSelectionChanged` already fills `m_selectedRoughness` from default state (`getSelectionState` no-selection branch).
- Prefer `onActivated` over `onCurrentIndexChanged` so programmatic `currentIndex` updates from D-Bus do not re-enter `setRoughness`.
- If PlasmaComponents ComboBox is preferred for visual consistency with sliders-only rows, `PlasmaComponents.ComboBox` may be used instead; tool/font rows already use `Controls.ComboBox`.

- [ ] **Step 2: Manual smoke (when overlay + plasmoid available)**

1. Start overlay daemon; open applet full representation.
2. No selection: ComboBox shows **Artist** (index 1).
3. Switch to Architect → draw a rectangle → stroke is neat (`roughness === 0`).
4. Select shape → change to Cartoonist → stroke updates without redraw gesture.
5. Press `S` in overlay → ComboBox index cycles 0→1→2→0 in lockstep.
6. Multi-select (if available) → ComboBox change updates all selected shapes’ `roughness`.

- [ ] **Step 3: Commit**

```bash
git add applet/contents/ui/FullRepresentation.qml
git commit -m "feat(applet): add Roughness combo (Architect/Artist/Cartoonist)"
```

---

### Task 3: Tests

**Files:**
- Modify: `tests/shapesmodeltest.cpp`

**Existing coverage to keep green:** `testRoughness()` (~542-585) already covers default, `updateProperties`, and `cycleRoughness`. Do not break it.

- [ ] **Step 1: Extend `testAppletBackendIntegration`**

After the block that asserts color/stroke/opacity defaults via backend (~lines 1233-1239), add roughness round-trip on defaults:

```cpp
    backend.setRoughness(2);
    QTest::qWait(50);
    QCOMPARE(controller.defaultRoughness(), 2);
    QCOMPARE(backend.selectedRoughness(), 2);

    backend.setRoughness(0);
    QTest::qWait(50);
    QCOMPARE(controller.defaultRoughness(), 0);
    QCOMPARE(backend.selectedRoughness(), 0);
```

After a shape is added and selected (~after line 1246 `QVERIFY(backend.hasSelection())`), assert selection path:

```cpp
    backend.setRoughness(2);
    QTest::qWait(50);
    QCOMPARE(controller.getSelectionState().value(QStringLiteral("roughness")).toInt(), 2);
    QCOMPARE(backend.selectedRoughness(), 2);
    QCOMPARE(controller.shapesModel()->shapes().at(0).value(QStringLiteral("roughness")).toInt(), 2);
```

Optional clamp check:

```cpp
    backend.setRoughness(99);
    QTest::qWait(50);
    QCOMPARE(backend.selectedRoughness(), 2); // clamped
```

- [ ] **Step 2: Run focused tests**

```bash
cmake --build build --target shapesmodeltest
ctest --test-dir build -R shapesmodeltest --output-on-failure
```

Expected: all tests pass, including `testRoughness` and `testAppletBackendIntegration`.

- [ ] **Step 3: Commit**

```bash
git add tests/shapesmodeltest.cpp
git commit -m "test: cover applet roughness selection and default round-trip"
```

---

### Task 4 (Optional): Clamp defaultRoughness on overlay

**Files:**
- Modify: `src/overlay/overlaycontroller.cpp` only

- [ ] **Step 1: Harden `setDefaultRoughness`**

Current (~240-246):

```cpp
void OverlayController::setDefaultRoughness(int roughness)
{
    if (m_defaultRoughness != roughness) {
        m_defaultRoughness = roughness;
        Q_EMIT defaultRoughnessChanged();
    }
}
```

Replace with:

```cpp
void OverlayController::setDefaultRoughness(int roughness)
{
    roughness = qBound(0, roughness, 2);
    if (m_defaultRoughness != roughness) {
        m_defaultRoughness = roughness;
        Q_EMIT defaultRoughnessChanged();
    }
}
```

Note: `cycleRoughness` already uses `% 3`, so clamp is defensive for D-Bus/applet only. Shape maps updated via `updateShape` still store the demarshalled value; if clamp-on-write for selected shapes is desired, clamp the demarshalled map entry inside `updateProperties` when key is `roughness` — keep that in the same optional commit if done.

- [ ] **Step 2: Commit**

```bash
git add src/overlay/overlaycontroller.cpp
git commit -m "fix(overlay): clamp defaultRoughness to 0..2"
```

---

## Test Plan (summary)

| Case | How |
|------|-----|
| Default no selection = 1 (Artist) | Unit: backend after connect / `getSelectionState` |
| `setRoughness` updates default | `testAppletBackendIntegration` |
| `setRoughness` updates selected shape | Unit after `addShape` |
| Hotkey `S` still cycles and refreshes applet | Manual + existing `testRoughness` |
| ComboBox labels map 0/1/2 | Manual |
| Out-of-range values clamped | Unit if Task 4 / backend `qBound` landed |
| Excalidraw paste still maps `roughness` | Existing paste tests; no regression expected |
| Build plugin + overlay + tests | `cmake --build` + `ctest` |

## Acceptance Criteria

1. FullRepresentation shows a Roughness control with three named levels: Architect, Artist, Cartoonist.
2. Changing the control with no selection updates `OverlayController::defaultRoughness` so new shapes inherit it.
3. Changing the control with a selection updates that shape’s `roughness` key and live rendering.
4. Applet control stays in sync when roughness changes via hotkey `S` or other `updateProperties` callers.
5. No new D-Bus interface name/signal is required; only existing `updateProperties` + `selectionChanged` payload key `roughness`.
6. `selectedRoughness` default is `1` (Artist), matching daemon.
7. Automated tests cover applet ↔ overlay roughness round-trip; existing `testRoughness` remains green.
8. Excalidraw clipboard behavior unchanged.

## Implementation Order

1. Task 1 (backend) — unblocks UI and tests  
2. Task 2 (QML) — user-visible control  
3. Task 3 (tests) — lock the contract  
4. Task 4 (optional clamp) — hardening  

## Risk Notes

- **Stale ComboBox index:** Use `onActivated`, not `onCurrentIndexChanged`, to avoid feedback loops when `selectionChanged` rewrites `currentIndex`.
- **Default mismatch:** Backend member default must stay `1`, not `0`, or the pre-connect UI flash will show Architect incorrectly.
- **Label width:** Longer “Roughness:” label may misalign the control column; adjust `gridUnit` multiplier if the panel looks uneven next to “Glow:”.
- **Live preview plan coordination:** If a separate live-preview-roughness plan adds draw-time preview, it should read `controller.defaultRoughness` (already set by this control) — no extra applet API needed.
