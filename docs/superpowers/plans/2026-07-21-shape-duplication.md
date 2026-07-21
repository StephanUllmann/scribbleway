# Shape Duplication (Ctrl+D) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Ctrl+D duplicate-in-place with a fixed ~(10,10)px offset for the current selection (single or multi), without touching the system clipboard.

**Architecture:** Implement `OverlayController::duplicateSelected()` by deep-cloning every shape with `selected == true`, applying a fixed geometry offset of `(10, 10)` using the same per-type geometry rules as `pasteFromClipboard`, appending clones via `ShapesModel::addShape` inside one `beginEdit`/`endEdit` transaction, deselecting originals, selecting the duplicates, and pointing `m_selectedIndex` at the last duplicate. Wire a QML `Shortcut` for `Ctrl+D`. No new model roles, D-Bus APIs, or Excalidraw keys.

**Tech Stack:** Qt 6, C++20, QML Shortcuts, Qt Test.

## Global Constraints

* Fixed offset: `+10` px X and `+10` px Y for every duplicated shape (and every freehand point).
* Do **not** write or read the clipboard; duplication is in-memory only.
* Preserve multi-selection: all selected shapes are duplicated; all originals become unselected; all duplicates become selected.
* One undo step: wrap the whole operation in `m_shapesModel.beginEdit()` / `endEdit()`.
* Deep-copy nested data (`points` list and any point maps) so clones do not share mutable structure with originals.
* New seeds: assign a fresh `seed` on each clone (random int, same style as draw finalize) so rough strokes do not look identical to the source.
* Locked flag on clones: set `locked` to `false` (same as paste) so the user can immediately move duplicates.
* No-op when nothing is selected.
* Excalidraw clipboard format is untouched; this feature has no serialization impact.

---

## File Structure & Decomposition

* `src/overlay/overlaycontroller.h` — declare `Q_INVOKABLE void duplicateSelected();`
* `src/overlay/overlaycontroller.cpp` — implement clone + offset + reselect (mirror geometry branches in `pasteFromClipboard` ~1062–1085)
* `src/overlay/qml/main.qml` — `Shortcut { sequence: "Ctrl+D"; ... controller.duplicateSelected() }` next to Copy/Paste
* `tests/shapesmodeltest.cpp` — unit tests for single, multi, freehand, undo, empty selection
* **No changes** to `ShapesModel` roles, applet backend, D-Bus, or Excalidraw converters (cloning is plain `QVariantMap` copy + key overrides)

Optional later (out of scope unless already touching shortcuts editor): add a rebindable `action_duplicate` entry to `m_localShortcuts`. Default plan uses a hard-coded `Ctrl+D` like Copy/Paste use `StandardKey`.

---

### Task 1: Controller API + Implementation

**Files:**
* Modify: `src/overlay/overlaycontroller.h` (after `copySelected` / `pasteFromClipboard`, ~96–97)
* Modify: `src/overlay/overlaycontroller.cpp` (after `pasteFromClipboard`, ~1098)
* Test: `tests/shapesmodeltest.cpp` (Task 2)

**Interfaces:**
* Consumes: `m_shapesModel.shapes()`, `addShape`, `beginEdit`/`endEdit`, `updateShape`, `notifyShapesChanged`, `notifySelectionChanged`, selection flag pattern from `pasteFromClipboard` (lines 1052–1097) and geometry offset branches (1062–1085)
* Produces: `Q_INVOKABLE void OverlayController::duplicateSelected()`

- [ ] **Step 1: Declare the invokable**

In `src/overlay/overlaycontroller.h`, immediately after `pasteFromClipboard`:

```cpp
    Q_INVOKABLE void copySelected();
    Q_INVOKABLE void pasteFromClipboard(double localX = -1.0, double localY = -1.0);
    Q_INVOKABLE void duplicateSelected();
    Q_INVOKABLE void selectShapesInRect(double rx, double ry, double rw, double rh, bool shiftHeld);
```

- [ ] **Step 2: Implement `duplicateSelected()`**

Add in `src/overlay/overlaycontroller.cpp` immediately after `pasteFromClipboard` (after the closing `}` near line 1098), before `convertToExcalidraw`:

```cpp
void OverlayController::duplicateSelected()
{
    // Collect selected shapes first (indices shift as we append).
    QList<QVariantMap> toClone;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        const QVariantMap &shape = m_shapesModel.shapes().at(i);
        if (shape.value(QStringLiteral("selected")).toBool()) {
            toClone.append(shape);
        }
    }
    if (toClone.isEmpty()) {
        return;
    }

    constexpr double kOffset = 10.0;

    m_shapesModel.beginEdit();

    // Deselect originals without leaving the edit transaction.
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (m_shapesModel.shapes().at(i).value(QStringLiteral("selected")).toBool()) {
            m_shapesModel.updateShape(i, {{QStringLiteral("selected"), false}});
        }
    }

    QList<int> newIndices;
    for (const QVariantMap &src : toClone) {
        QVariantMap shape = src; // QVariantMap is implicitly shared; we overwrite all mutated keys
        const QString type = shape.value(QStringLiteral("type")).toString();

        shape.insert(QStringLiteral("selected"), true);
        shape.insert(QStringLiteral("locked"), false);
        // Fresh seed so rough strokes diverge from the source
        shape.insert(QStringLiteral("seed"),
                     QRandomGenerator::global()->bounded(1, 1000001));

        if (type == QStringLiteral("rectangle")
            || type == QStringLiteral("ellipse")
            || type == QStringLiteral("text")) {
            shape.insert(QStringLiteral("x"),
                         shape.value(QStringLiteral("x")).toDouble() + kOffset);
            shape.insert(QStringLiteral("y"),
                         shape.value(QStringLiteral("y")).toDouble() + kOffset);
        } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
            shape.insert(QStringLiteral("fromX"),
                         shape.value(QStringLiteral("fromX")).toDouble() + kOffset);
            shape.insert(QStringLiteral("toX"),
                         shape.value(QStringLiteral("toX")).toDouble() + kOffset);
            shape.insert(QStringLiteral("fromY"),
                         shape.value(QStringLiteral("fromY")).toDouble() + kOffset);
            shape.insert(QStringLiteral("toY"),
                         shape.value(QStringLiteral("toY")).toDouble() + kOffset);
        } else if (type == QStringLiteral("freehand")) {
            QVariantList points = shape.value(QStringLiteral("points")).toList();
            QVariantList newPoints;
            newPoints.reserve(points.size());
            for (const QVariant &pv : points) {
                if (pv.canConvert<QPointF>()) {
                    const QPointF p = pv.toPointF();
                    newPoints.append(QPointF(p.x() + kOffset, p.y() + kOffset));
                } else if (pv.typeId() == QMetaType::QVariantMap) {
                    QVariantMap pm = pv.toMap();
                    pm.insert(QStringLiteral("x"),
                              pm.value(QStringLiteral("x")).toDouble() + kOffset);
                    pm.insert(QStringLiteral("y"),
                              pm.value(QStringLiteral("y")).toDouble() + kOffset);
                    newPoints.append(pm);
                }
            }
            shape.insert(QStringLiteral("points"), newPoints);
        }

        m_shapesModel.addShape(shape);
        newIndices.append(m_shapesModel.rowCount() - 1);
    }

    if (!newIndices.isEmpty()) {
        m_selectedIndex = newIndices.last();
    }

    m_shapesModel.endEdit();
    notifyShapesChanged();
    notifySelectionChanged();
}
```

Include if missing at top of `overlaycontroller.cpp`:

```cpp
#include <QRandomGenerator>
```

(`QGuiApplication`, `QJson*`, etc. already present for copy/paste.)

**Notes for implementer:**
* Do **not** call `setSelectedIndex(-1)` first the way paste does if that would open nested edit transactions incorrectly; the explicit `updateShape(..., selected:false)` loop above stays inside one `beginEdit`/`endEdit`. `ShapesModel::updateShape` already skips history for pure `selected` flips when not otherwise dirty — still safe inside a transaction.
* `addShape` itself calls `saveHistorySnapshot` unless `m_inEditTransaction` / undo path — `beginEdit` coalesces.
* Relative positions between multi-selected shapes are preserved because every clone gets the same absolute offset.

- [ ] **Step 3: Build overlay target**

Run:

```bash
cmake --build build --target scribbleway-overlay -j$(nproc)
```

Expected: compiles cleanly with `duplicateSelected` linked.

- [ ] **Step 4: Commit**

```bash
git add src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp
git commit -m "feat: add OverlayController::duplicateSelected with 10px offset"
```

---

### Task 2: Unit Tests

**Files:**
* Modify: `tests/shapesmodeltest.cpp`
* Consumes: `OverlayController::duplicateSelected()` from Task 1

**Interfaces:**
* Produces: `testDuplicateSelected` (or similarly named) covering single, multi, freehand, empty, undo

- [ ] **Step 1: Add failing tests**

Locate the existing clipboard test block (~`testCopyPaste` around lines 320–380) and add a new private slot declaration in the test class header section, then implement:

```cpp
void ShapesModelTest::testDuplicateSelected()
{
    OverlayController controller;

    // --- empty selection is a no-op ---
    controller.duplicateSelected();
    QCOMPARE(controller.shapesModel()->rowCount(), 0);

    // --- single rectangle ---
    QVariantMap rect;
    rect[QStringLiteral("type")] = QStringLiteral("rectangle");
    rect[QStringLiteral("x")] = 100.0;
    rect[QStringLiteral("y")] = 200.0;
    rect[QStringLiteral("width")] = 50.0;
    rect[QStringLiteral("height")] = 40.0;
    rect[QStringLiteral("color")] = QStringLiteral("#ff0000");
    rect[QStringLiteral("strokeWidth")] = 3;
    rect[QStringLiteral("opacity")] = 0.8;
    rect[QStringLiteral("borderRadius")] = 6;
    rect[QStringLiteral("roughness")] = 2;
    rect[QStringLiteral("seed")] = 42;
    rect[QStringLiteral("glow")] = 5;
    rect[QStringLiteral("selected")] = true;
    rect[QStringLiteral("locked")] = true;
    controller.addShape(rect);
    QCOMPARE(controller.shapesModel()->rowCount(), 1);
    QCOMPARE(controller.selectedIndex(), 0);

    controller.duplicateSelected();
    QCOMPARE(controller.shapesModel()->rowCount(), 2);

    const QVariantMap orig = controller.shapesModel()->shapes().at(0);
    const QVariantMap dup = controller.shapesModel()->shapes().at(1);

    QCOMPARE(orig.value(QStringLiteral("selected")).toBool(), false);
    QCOMPARE(dup.value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(controller.selectedIndex(), 1);

    QCOMPARE(dup.value(QStringLiteral("x")).toDouble(), 110.0);
    QCOMPARE(dup.value(QStringLiteral("y")).toDouble(), 210.0);
    QCOMPARE(dup.value(QStringLiteral("width")).toDouble(), 50.0);
    QCOMPARE(dup.value(QStringLiteral("height")).toDouble(), 40.0);
    QCOMPARE(dup.value(QStringLiteral("color")).toString(), QStringLiteral("#ff0000"));
    QCOMPARE(dup.value(QStringLiteral("strokeWidth")).toInt(), 3);
    QCOMPARE(dup.value(QStringLiteral("opacity")).toDouble(), 0.8);
    QCOMPARE(dup.value(QStringLiteral("borderRadius")).toInt(), 6);
    QCOMPARE(dup.value(QStringLiteral("roughness")).toInt(), 2);
    QCOMPARE(dup.value(QStringLiteral("glow")).toInt(), 5);
    QCOMPARE(dup.value(QStringLiteral("locked")).toBool(), false);
    // seed must change (not equal to source 42)
    QVERIFY(dup.value(QStringLiteral("seed")).toInt() != 42);

    // Original geometry unchanged
    QCOMPARE(orig.value(QStringLiteral("x")).toDouble(), 100.0);
    QCOMPARE(orig.value(QStringLiteral("y")).toDouble(), 200.0);
    QCOMPARE(orig.value(QStringLiteral("locked")).toBool(), true);

    // --- undo restores pre-duplicate state ---
    controller.undo();
    QCOMPARE(controller.shapesModel()->rowCount(), 1);
    QCOMPARE(controller.shapesModel()->shapes().at(0).value(QStringLiteral("x")).toDouble(), 100.0);

    // Re-select original for multi-select path (undo may leave selection state
    // depending on snapshot; force clean selection)
    controller.clear();

    // --- multi-select: rect + line ---
    QVariantMap r2;
    r2[QStringLiteral("type")] = QStringLiteral("rectangle");
    r2[QStringLiteral("x")] = 10.0;
    r2[QStringLiteral("y")] = 20.0;
    r2[QStringLiteral("width")] = 30.0;
    r2[QStringLiteral("height")] = 40.0;
    r2[QStringLiteral("color")] = QStringLiteral("#00ff00");
    r2[QStringLiteral("selected")] = false;
    controller.addShape(r2);

    QVariantMap line;
    line[QStringLiteral("type")] = QStringLiteral("line");
    line[QStringLiteral("fromX")] = 0.0;
    line[QStringLiteral("fromY")] = 0.0;
    line[QStringLiteral("toX")] = 100.0;
    line[QStringLiteral("toY")] = 50.0;
    line[QStringLiteral("color")] = QStringLiteral("#0000ff");
    line[QStringLiteral("selected")] = false;
    controller.addShape(line);

    // Select both (shift)
    controller.selectShape(0, false);
    controller.selectShape(1, true);
    QVERIFY(controller.hasMultiSelection());

    controller.duplicateSelected();
    QCOMPARE(controller.shapesModel()->rowCount(), 4);

    // Originals unselected
    QCOMPARE(controller.shapesModel()->shapes().at(0).value(QStringLiteral("selected")).toBool(), false);
    QCOMPARE(controller.shapesModel()->shapes().at(1).value(QStringLiteral("selected")).toBool(), false);
    // Duplicates selected
    QCOMPARE(controller.shapesModel()->shapes().at(2).value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(3).value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(controller.selectedIndex(), 3);

    const QVariantMap rDup = controller.shapesModel()->shapes().at(2);
    const QVariantMap lDup = controller.shapesModel()->shapes().at(3);
    QCOMPARE(rDup.value(QStringLiteral("x")).toDouble(), 20.0);
    QCOMPARE(rDup.value(QStringLiteral("y")).toDouble(), 30.0);
    QCOMPARE(lDup.value(QStringLiteral("fromX")).toDouble(), 10.0);
    QCOMPARE(lDup.value(QStringLiteral("fromY")).toDouble(), 10.0);
    QCOMPARE(lDup.value(QStringLiteral("toX")).toDouble(), 110.0);
    QCOMPARE(lDup.value(QStringLiteral("toY")).toDouble(), 60.0);

    // Relative gap preserved: original dx between rect.x and line.fromX was 10-0=10;
    // after offset still 20-10=10
    QCOMPARE(rDup.value(QStringLiteral("x")).toDouble() - lDup.value(QStringLiteral("fromX")).toDouble(), 10.0);

    controller.clear();

    // --- freehand points offset ---
    QVariantMap fh;
    fh[QStringLiteral("type")] = QStringLiteral("freehand");
    fh[QStringLiteral("color")] = QStringLiteral("#ffffff");
    QVariantList pts;
    pts.append(QPointF(5.0, 5.0));
    pts.append(QPointF(15.0, 25.0));
    fh[QStringLiteral("points")] = pts;
    fh[QStringLiteral("selected")] = true;
    controller.addShape(fh);

    controller.duplicateSelected();
    QCOMPARE(controller.shapesModel()->rowCount(), 2);
    const QVariantList dPts =
        controller.shapesModel()->shapes().at(1).value(QStringLiteral("points")).toList();
    QCOMPARE(dPts.size(), 2);
    QCOMPARE(dPts.at(0).toPointF(), QPointF(15.0, 15.0));
    QCOMPARE(dPts.at(1).toPointF(), QPointF(25.0, 35.0));
    // Source points untouched
    const QVariantList sPts =
        controller.shapesModel()->shapes().at(0).value(QStringLiteral("points")).toList();
    QCOMPARE(sPts.at(0).toPointF(), QPointF(5.0, 5.0));
}
```

Also declare the slot in the test class (near other `void test...` private slots):

```cpp
    void testDuplicateSelected();
```

- [ ] **Step 2: Run tests — expect FAIL before implementation is present, PASS after Task 1**

```bash
cmake --build build --target shapesmodeltest -j$(nproc)
ctest --test-dir build -R shapesmodeltest --output-on-failure
# or directly:
./build/bin/shapesmodeltest testDuplicateSelected
# path may be build/tests/shapesmodeltest — use whatever the project produces
```

Expected after Task 1: `testDuplicateSelected` PASS; full suite still green.

If the binary path differs, discover with:

```bash
find build -name 'shapesmodeltest' -type f
```

- [ ] **Step 3: Commit tests**

```bash
git add tests/shapesmodeltest.cpp
git commit -m "test: cover duplicateSelected single, multi, freehand, undo"
```

---

### Task 3: QML Shortcut

**Files:**
* Modify: `src/overlay/qml/main.qml` (~583–597, after Paste shortcut)

**Interfaces:**
* Consumes: `controller.duplicateSelected()`
* Produces: `Ctrl+D` binding, disabled while text editor is open (same guard as Copy/Delete)

- [ ] **Step 1: Add Shortcut**

Immediately after the Paste `Shortcut` block:

```qml
    Shortcut {
        sequence: StandardKey.Paste
        enabled: !textEditor.visible
        onActivated: {
            controller.pasteFromClipboard(canvasWindow.lastMousePos.x, canvasWindow.lastMousePos.y);
        }
    }

    Shortcut {
        sequence: "Ctrl+D"
        enabled: !textEditor.visible
        onActivated: {
            controller.duplicateSelected();
        }
    }
```

Rationale for hard-coded `"Ctrl+D"`: matches common drawing-app UX; Copy/Paste already use `StandardKey` rather than `localShortcutSequences`. Optional follow-up (not this plan): register `action_duplicate` in `m_localShortcuts` constructor list (`overlaycontroller.cpp` ~39–55) and bind `sequence: controller.localShortcutSequences["action_duplicate"]` with default `"Ctrl+D"` so the applet shortcut editor can rebind it.

- [ ] **Step 2: Smoke-check (manual)**

Run overlay (dev install or `scribbleway-overlay` from build), enter select mode, draw a rectangle, select it, press Ctrl+D:

* Duplicate appears 10px down-right
* Duplicate is selected; original is not
* Second Ctrl+D offsets again from the new selection
* Ctrl+Z undoes the last duplication as one step
* Multi-select two shapes → Ctrl+D clones both with relative layout preserved

- [ ] **Step 3: Commit**

```bash
git add src/overlay/qml/main.qml
git commit -m "feat: bind Ctrl+D to duplicateSelected in overlay QML"
```

---

## Test Plan

| Case | Setup | Action | Expect |
|------|--------|--------|--------|
| Empty | No shapes | `duplicateSelected()` | rowCount stays 0 |
| Single rect | One selected rect at (100,200) | duplicate | New shape (110,210); orig unselected; `selectedIndex` = last |
| Locked source | Source `locked=true` | duplicate | Clone `locked=false`; source still locked |
| Properties | color/stroke/opacity/glow/roughness/borderRadius set | duplicate | All copied; `seed` different |
| Multi | Two selected (rect+line) | duplicate | 4 shapes; both clones selected; relative offset preserved |
| Freehand | points `[(5,5),(15,25)]` | duplicate | Clone points `[(15,15),(25,35)]`; source unchanged |
| Line/arrow | from/to coords | duplicate | from/to each +10 |
| Text/ellipse | x/y box | duplicate | x/y +10 |
| Undo | After duplicate | `undo()` | rowCount and geometry restored to pre-duplicate snapshot |
| Clipboard isolation | Clipboard has unrelated text | duplicate | Clipboard text unchanged |
| Shortcut | UI, text editor closed | Ctrl+D | Same as API |
| Text edit open | `textEditor.visible` | Ctrl+D | Shortcut disabled (no accidental dup while typing) |

Automated coverage: Task 2. Manual: Task 3 Step 2.

---

## Excalidraw Compatibility Impact

**None.** Duplication never calls `convertToExcalidraw` / `convertFromExcalidraw` and does not alter clipboard JSON. Cloned maps keep Scribbleway-only keys (`glow`, etc.) as plain `QVariantMap` copies. Paste/copy paths remain the sole Excalidraw bridge.

---

## Acceptance Criteria

1. `OverlayController` exposes `Q_INVOKABLE void duplicateSelected()`.
2. With one or more shapes selected, calling it (or pressing Ctrl+D with the overlay focused and text editor hidden) appends clones offset by exactly `(+10, +10)` on all geometry channels used by that type (`x/y`, `fromX/fromY/toX/toY`, or each freehand point).
3. Originals are deselected; duplicates are selected; `selectedIndex` is the last duplicate index.
4. Operation is a single undo step.
5. Nested `points` are deep-copied (mutating clone points must not change source points).
6. Clone `seed` ≠ source `seed`; clone `locked == false`.
7. No clipboard read/write.
8. `testDuplicateSelected` (or equivalent) passes in `shapesmodeltest`.
9. No new D-Bus methods, ShapeRoles, or Q_PROPERTYs required.

---

## Self-Review

* Spec “clone selected, offset ~(10,10), add, select duplicate” → Tasks 1–3.
* Multi-select, undo, freehand deep copy, locked/seed behavior spelled out with code.
* No placeholders / TBD.
* Types match existing API: `QVariantMap` shapes, `beginEdit`/`endEdit`, `notifyShapesChanged`/`notifySelectionChanged`.
* ShapesModel itself needs no new method; controller owns the feature (consistent with copy/paste living on the controller).
