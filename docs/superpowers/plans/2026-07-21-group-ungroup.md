# Group / Ungroup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add persistent shape groups so multi-selected shapes can be grouped, then moved / deleted / raised / lowered / locked as one unit, with Ctrl+G / Ctrl+Shift+G shortcuts and Excalidraw-compatible clipboard `groupIds`.

**Architecture:** Prefer a flat `groupId` field on each shape (`QVariantMap` key + `GroupIdRole`) over a nested group shape type. Nested containers would force a new QML delegate, z-order model rewrite, and break the current flat `QList<QVariantMap>` / Repeater design. A shared non-empty `groupId` string is enough: selection expands to the whole group, drag/delete/lock already iterate `selected`, and raise/lower move contiguous group blocks.

**Tech Stack:** Qt 6, C++20, QML, D-Bus, Qt Test.

## Motivation

Multi-selection already exists (`selectShape(..., shiftHeld)`, marquee via `selectShapesInRect`, `dragSelected`, `deleteSelected`, `updateProperties` over all `selected` shapes). It is ephemeral: next plain click collapses selection to one shape. There is no way to permanently bind shapes so they keep acting as one unit after deselect/reselect.

## Design

### Data model

| Key | Type | Meaning |
|---|---|---|
| `groupId` | `QString` | Empty / missing = ungrouped. Non-empty UUID (WithoutBraces, 8+ chars OK) shared by all members of one group. |

- No nested groups in v1. Grouping shapes that already belong to different groups merges them into one new `groupId`.
- Ungroup clears `groupId` on every selected member of each touched group (not only the primary index).
- `groupId` is **not** part of history-exempt keys (unlike `selected`). Changing it must snapshot undo via existing `beginEdit` / `updateShape` real-change path.
- Do **not** introduce a `type: "group"` shape.

### Behavioral contract

1. **Select expands to group.** Clicking any member (no Shift) selects **all** shapes with the same non-empty `groupId`. Shift-click still toggles only the clicked shape (allows partial ungroup prep / multi-group ops); optional follow-up: Shift-click expands toggle to whole group — implement expansion on plain click only in v1.
2. **Marquee does not auto-expand.** `selectShapesInRect` keeps geometric selection; users can then Group.
3. **Drag / property edit / delete / lock.** Already operate on every `selected` shape. After select-expansion, these work for groups with no extra geometry code. `beginEdit()` already snapshots all selected into `m_dragStartShapes` (`overlaycontroller.cpp` ~473–481).
4. **Raise / lower move the whole group as a contiguous z-block.** Today `raiseSelected` / `lowerSelected` only move `m_selectedIndex` by one (`overlaycontroller.cpp` ~581–600). After grouping, members may be non-contiguous; group ops must:
   - Collect all indices sharing any selected shape’s `groupId` (and plain selected ungrouped indices).
   - For a single group: extract members preserving relative order, reinsert as one block one step up/down past the nearest outsider.
   - Multi-selection of multiple groups + ungrouped shapes: raise/lower each contiguous selected run, or only the primary group of `m_selectedIndex` — **v1 rule:** operate on the union of all groups that intersect the current selection plus any selected ungrouped shapes, moving them as one combined ordered block.
5. **Copy / paste.** `convertToExcalidraw` writes Excalidraw `groupIds: [groupId]` when non-empty. `convertFromExcalidraw` reads `groupIds[0]` into `groupId`. On paste, remap each distinct old id → fresh UUID so pasted groups don’t collide with canvas groups (`pasteFromClipboard` ~1052–1097).
6. **Undo.** `groupSelected` / `ungroupSelected` wrap in `m_shapesModel.beginEdit()` / `endEdit()` so one Ctrl+Z restores prior ids.

### API surface

**`ShapesModel`**
- `GroupIdRole` after `GlowRole`
- role name `"groupId"`
- `data()` / `roleNames()` / `updateShape()` key→role map

**`OverlayController`**
```cpp
Q_INVOKABLE void groupSelected();
Q_INVOKABLE void ungroupSelected();
// private helpers:
QStringList groupIdsForSelection() const;
QList<int> indicesForGroupId(const QString &groupId) const;
QList<int> expandIndicesToGroups(const QList<int> &indices) const;
void selectIndices(const QList<int> &indices, int primaryIndex);
bool moveIndexBlock(const QList<int> &sortedIndices, int direction); // +1 raise, -1 lower
```

Optional Q_PROPERTY (nice for applet button enablement, not strictly required if applet uses selectionState):
```cpp
Q_PROPERTY(bool canGroup READ canGroup NOTIFY selectionChanged)
Q_PROPERTY(bool canUngroup READ canUngroup NOTIFY selectionChanged)
```
- `canGroup`: ≥2 selected shapes (after optional expand, still ≥2 distinct indices).
- `canUngroup`: ≥1 selected shape with non-empty `groupId`.

**Selection state D-Bus map** (`getSelectionState`, ~374–406): add
- `groupId` (primary shape’s id, or `""`)
- `canGroup` (bool)
- `canUngroup` (bool)

**Local shortcuts** (`m_localShortcuts` ctor list ~39–61):
- `{ "action_group", "Group", "Ctrl+G", "Ctrl+G" }`
- `{ "action_ungroup", "Ungroup", "Ctrl+Shift+G", "Ctrl+Shift+G" }`

**QML** `main.qml`: two `Shortcut` entries bound to `controller.localShortcutSequences["action_group"|"action_ungroup"]`, `enabled: canvasWindow.shortcutGuard`.

**Applet (in scope for complete UX):**
- `AppletBackend::groupSelected()` / `ungroupSelected()` D-Bus forwarders
- Buttons next to Raise/Lower in `FullRepresentation.qml` selection row (~166–188)

**BaseShape.qml:** no structural change required if expansion is server-side in `selectShape`. Keep per-shape selection chrome. Resize handles already hidden when `hasMultiSelection` (`BaseShape.qml` ~154) — groups with ≥2 members keep that. Optional: thicker border when `model.groupId` non-empty — skip in v1.

### Excalidraw compatibility impact

| Direction | Behavior |
|---|---|
| Scribbleway → clipboard | If `groupId` non-empty, emit `"groupIds": [ "<id>" ]` on the element JSON. Empty → omit or `[]` (prefer omit / empty array; Excalidraw accepts both). |
| clipboard → Scribbleway | Read first entry of `groupIds` array into `groupId`. Missing/empty → ungrouped. |
| Frame / boundElements / containerId | **Out of scope.** Do not invent frame shapes. |
| Nested `groupIds` (Excalidraw allows multiple) | v1 stores only `groupIds[0]`; on export write single-id array. Document as known limitation. |

No changes to visual roughness/glow path generators.

---

## File Structure & Decomposition

| File | Change |
|---|---|
| `src/overlay/shapesmodel.h` | `GroupIdRole` |
| `src/overlay/shapesmodel.cpp` | role get/set mapping |
| `src/overlay/overlaycontroller.h` | slots, helpers, optional canGroup/canUngroup |
| `src/overlay/overlaycontroller.cpp` | group/ungroup, select expand, raise/lower block move, clipboard groupIds + paste remap, shortcuts, selectionState |
| `src/overlay/qml/main.qml` | default `groupId: ""` on new shapes; Ctrl+G / Ctrl+Shift+G shortcuts |
| `src/overlay/qml/shapes/BaseShape.qml` | optional `modelGroupId` alias only if needed later — **no required change** |
| `src/applet-plugin/appletbackend.h/.cpp` | `groupSelected` / `ungroupSelected` invokables |
| `applet/contents/ui/FullRepresentation.qml` | Group / Ungroup buttons |
| `tests/shapesmodeltest.cpp` | `testGroupUngroup` (+ role smoke if useful) |

---

### Task 1: Data model — `GroupIdRole`

**Files:**
- Modify: `src/overlay/shapesmodel.h`
- Modify: `src/overlay/shapesmodel.cpp`

**Interfaces:**
- Produces: `ShapesModel::GroupIdRole` ↔ `"groupId"`

- [ ] **Step 1: Enum**

In `shapesmodel.h` after `GlowRole` (~line 36):
```cpp
        GlowRole,
        GroupIdRole
```

- [ ] **Step 2: `data()` / `roleNames()` / `updateShape()`**

`shapesmodel.cpp` `data()` after Glow case (~65):
```cpp
        case GlowRole: return shape.value(QStringLiteral("glow"));
        case GroupIdRole: return shape.value(QStringLiteral("groupId"));
```

`roleNames()` after glow (~95):
```cpp
    roles[GlowRole] = "glow";
    roles[GroupIdRole] = "groupId";
```

`updateShape()` role map after glow (~208):
```cpp
                else if (it.key() == QStringLiteral("glow")) changedRoles << GlowRole;
                else if (it.key() == QStringLiteral("groupId")) changedRoles << GroupIdRole;
```

Keep `groupId` as a **real** change (do **not** add it to the `selected`-only history exemption at ~167).

- [ ] **Step 3: Build**

```bash
cmake --build build --target scribbleway-overlay
```
Expected: success.

- [ ] **Step 4: Commit**

```bash
git add src/overlay/shapesmodel.h src/overlay/shapesmodel.cpp
git commit -m "feat(model): add groupId role to ShapesModel"
```

---

### Task 2: Controller — group / ungroup + selection expansion

**Files:**
- Modify: `src/overlay/overlaycontroller.h`
- Modify: `src/overlay/overlaycontroller.cpp`

**Interfaces:**
- Produces: `groupSelected()`, `ungroupSelected()`, group-aware `selectShape`
- Consumes: `ShapesModel::updateShape`, `beginEdit`/`endEdit`

- [ ] **Step 1: Declarations in `overlaycontroller.h`**

Public slots (near `raiseSelected` ~126):
```cpp
    void groupSelected();
    void ungroupSelected();
```

Optional properties (near `hasMultiSelection` ~45):
```cpp
    Q_PROPERTY(bool canGroup READ canGroup NOTIFY selectionChanged)
    Q_PROPERTY(bool canUngroup READ canUngroup NOTIFY selectionChanged)
```
```cpp
    bool canGroup() const;
    bool canUngroup() const;
```

Private helpers (near `notifySelectionChanged` ~162):
```cpp
    QList<int> selectedIndices() const;
    QList<int> expandIndicesToGroups(const QList<int> &indices) const;
    QList<int> indicesForGroupId(const QString &groupId) const;
    void applySelection(const QSet<int> &selected, int primaryIndex);
```

- [ ] **Step 2: Helpers in `overlaycontroller.cpp`**

```cpp
QList<int> OverlayController::selectedIndices() const
{
    QList<int> out;
    const auto shapes = m_shapesModel.shapes();
    for (int i = 0; i < shapes.size(); ++i) {
        if (shapes[i].value(QStringLiteral("selected"), false).toBool())
            out.append(i);
    }
    return out;
}

QList<int> OverlayController::indicesForGroupId(const QString &groupId) const
{
    QList<int> out;
    if (groupId.isEmpty())
        return out;
    const auto shapes = m_shapesModel.shapes();
    for (int i = 0; i < shapes.size(); ++i) {
        if (shapes[i].value(QStringLiteral("groupId")).toString() == groupId)
            out.append(i);
    }
    return out;
}

QList<int> OverlayController::expandIndicesToGroups(const QList<int> &indices) const
{
    QSet<int> expanded;
    const auto shapes = m_shapesModel.shapes();
    for (int idx : indices) {
        if (idx < 0 || idx >= shapes.size())
            continue;
        const QString gid = shapes[idx].value(QStringLiteral("groupId")).toString();
        if (gid.isEmpty()) {
            expanded.insert(idx);
        } else {
            for (int gIdx : indicesForGroupId(gid))
                expanded.insert(gIdx);
        }
    }
    QList<int> out = expanded.values();
    std::sort(out.begin(), out.end());
    return out;
}

void OverlayController::applySelection(const QSet<int> &selected, int primaryIndex)
{
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        const bool want = selected.contains(i);
        if (m_shapesModel.shapes()[i].value(QStringLiteral("selected"), false).toBool() != want)
            m_shapesModel.updateShape(i, {{QStringLiteral("selected"), want}});
    }
    m_selectedIndex = selected.contains(primaryIndex) ? primaryIndex
                      : (selected.isEmpty() ? -1 : *std::max_element(selected.begin(), selected.end()));
}
```

`canGroup` / `canUngroup`:
```cpp
bool OverlayController::canGroup() const
{
    return selectedIndices().size() >= 2;
}

bool OverlayController::canUngroup() const
{
    const auto shapes = m_shapesModel.shapes();
    for (int i : selectedIndices()) {
        if (!shapes[i].value(QStringLiteral("groupId")).toString().isEmpty())
            return true;
    }
    return false;
}
```

- [ ] **Step 3: `groupSelected()` / `ungroupSelected()`**

```cpp
void OverlayController::groupSelected()
{
    QList<int> indices = selectedIndices();
    if (indices.size() < 2)
        return;

    // Merge: include every member of any group already represented in the selection
    indices = expandIndicesToGroups(indices);
    if (indices.size() < 2)
        return;

    const QString newId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    m_shapesModel.beginEdit();
    for (int i : indices) {
        m_shapesModel.updateShape(i, {{QStringLiteral("groupId"), newId}});
    }
    // Keep all members selected
    QSet<int> sel(indices.begin(), indices.end());
    applySelection(sel, m_selectedIndex >= 0 ? m_selectedIndex : indices.last());
    m_shapesModel.endEdit();

    notifyShapesChanged();
    notifySelectionChanged();
}

void OverlayController::ungroupSelected()
{
    QList<int> indices = expandIndicesToGroups(selectedIndices());
    bool any = false;
    for (int i : indices) {
        if (!m_shapesModel.shapes()[i].value(QStringLiteral("groupId")).toString().isEmpty()) {
            any = true;
            break;
        }
    }
    if (!any)
        return;

    m_shapesModel.beginEdit();
    for (int i : indices) {
        if (!m_shapesModel.shapes()[i].value(QStringLiteral("groupId")).toString().isEmpty())
            m_shapesModel.updateShape(i, {{QStringLiteral("groupId"), QString()}});
    }
    m_shapesModel.endEdit();

    notifyShapesChanged();
    notifySelectionChanged();
}
```

Include `<QUuid>` and `<algorithm>` if not already present in the cpp.

- [ ] **Step 4: Expand plain click in `selectShape`**

Replace the non-shift branch of `selectShape` (~664–684) so that when `!shiftHeld` and the target has a non-empty `groupId`, all group members become selected:

```cpp
    } else {
        // Plain click: select target, expanding to full group when grouped
        QList<int> toSelect = expandIndicesToGroups({index});
        m_shapesModel.beginEdit();
        QSet<int> sel(toSelect.begin(), toSelect.end());
        applySelection(sel, index);
        m_shapesModel.endEdit();
        ensureSelectMode();
        notifyShapesChanged();
        notifySelectionChanged();
    }
```

When target was already selected and is the only selected shape / already full group, keep current short-circuit if desired (avoid spurious history). Prefer: if `wasSelected && !hasMultiSelection()` and group already fully selected, only update `m_selectedIndex` — mirror existing early path at ~665–669 for the single-shape case; for groups, if all members already selected and click is inside group, just set primary index without `beginEdit`.

Shift branch (~644–663): leave as single-index toggle (v1).

- [ ] **Step 5: `getSelectionState` keys**

In both branches of `getSelectionState` (~374–406), add:
```cpp
        state[QStringLiteral("groupId")] = /* primary shape groupId or "" */;
        state[QStringLiteral("canGroup")] = canGroup();
        state[QStringLiteral("canUngroup")] = canUngroup();
```

- [ ] **Step 6: Default shape creation path**

In `main.qml` `finalizeShape()` (~120–130), add:
```javascript
            "groupId": "",
```
(Controller `addShape` callers in tests may omit it; missing ≡ ungrouped.)

- [ ] **Step 7: Build + commit**

```bash
cmake --build build --target scribbleway-overlay
git add src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp src/overlay/qml/main.qml
git commit -m "feat: groupSelected/ungroupSelected and group-expanding selection"
```

---

### Task 3: Raise / lower group blocks

**Files:**
- Modify: `src/overlay/overlaycontroller.cpp` (`raiseSelected`, `lowerSelected`)
- Possibly: `src/overlay/shapesmodel.h/.cpp` if a bulk reorder helper is cleaner

**Design detail:**

Current API `moveShape(from, to)` moves one row (`shapesmodel.cpp` ~231–247). For a group with indices e.g. `[1,3,4]` raising once:

1. Let `block = expandIndicesToGroups(selectedIndices())` sorted ascending.
2. If `block` empty, return.
3. **Raise (`direction = +1`):** if `block.last() == rowCount()-1`, no-op. Target insert position = `block.last() + 2` in “remove first then insert” indexing — implement carefully:
   - Snapshot `QList<QVariantMap> moved` in ascending index order.
   - Remove from high index to low via `removeShape` **without** individual history (already inside one `beginEdit`).
   - Compute `insertAt = block.last() + 1 - (block.size() - 1)` after removals… simpler algorithm:

**Recommended algorithm (`moveIndexBlock`):**
```text
sorted = block ascending
if direction > 0:  # raise (toward end / higher z)
  if sorted.last() >= rowCount-1: return false
  # swap block as a unit with the single element immediately above the block top
  boundary = sorted.last() + 1
  # extract shapes at sorted indices; extract boundary shape
  # rewrite m_shapes order: [... before block ...][boundary][block...][after]
else:  # lower
  if sorted.first() <= 0: return false
  boundary = sorted.first() - 1
  # [... before ...][block...][boundary][after]
```

Because `ShapesModel` does not expose arbitrary reorder, either:

**Option A (preferred, small model API):** add
```cpp
bool ShapesModel::reorderShapes(const QList<int> &fromOrderToNewPositions);
// or
void ShapesModel::setShapesOrder(const QList<QVariantMap> &shapes); // already have setShapes — but it snapshots history again
```
Add a non-history `replaceShapesInEdit(const QList<QVariantMap> &)` used only inside an active edit transaction, **or** implement raise/lower by repeated `moveShape` on each member from top/bottom so relative order is preserved:

**Option B (no model API change — use repeated `moveShape`):**
```cpp
// Raise block: move highest index up once, then next-highest, ... preserving order
// For contiguous block [i..j], moveShape(j, j+1) once is enough if contiguous.
// For non-contiguous, first compact block to contiguous near its max, then move — too heavy.
```

**v1 simplification (document in code):** On `groupSelected`, **also compact** members into a contiguous z-run at the position of the highest member (stable relative order). Then raise/lower only needs to move a contiguous range:

```cpp
void OverlayController::compactGroupIndices(const QList<int> &sortedIndices)
{
    // Pull members out (high→low), reinsert as a block ending at original max index
    // Must run inside beginEdit; use model moveShape repeatedly:
    // Target: final indices [max - n + 1, ..., max] with same relative order.
}
```

Call `compactGroupIndices` at end of `groupSelected()` so groups are always contiguous. Then:

```cpp
void OverlayController::raiseSelected()
{
    QList<int> block = expandIndicesToGroups(selectedIndices());
    if (block.isEmpty()) return;
    // assume contiguous after compact on group; if not contiguous (legacy), compact first
    if (!isContiguous(block))
        compactSelectionBlock(block), block = expandIndicesToGroups(selectedIndices());

    if (block.last() >= m_shapesModel.rowCount() - 1) return;

    m_shapesModel.beginEdit();
    // Move top element up, then the rest follow by moving each from high to low:
    // For contiguous [lo..hi], moveShape(hi, hi+1) once elevates whole block if we
    // move hi, then hi-1, ... lo each +1:
    for (int i = block.size() - 1; i >= 0; --i) {
        int from = block[i] + (block.size() - 1 - i); // adjust as prior moves shift indices
        // clearer: iterate k from hi down to lo: moveShape(k, k+1)
    }
    // After loop, update m_selectedIndex to new primary
    m_shapesModel.endEdit();
    notifyShapesChanged();
    notifySelectionChanged();
}
```

Concrete contiguous raise implementation:
```cpp
// block contiguous [lo, hi]
m_shapesModel.beginEdit();
for (int k = hi; k >= lo; --k) {
    m_shapesModel.moveShape(k, k + 1);
}
m_selectedIndex = /* map old primary through +1 if primary in block */;
// re-select all moved indices [lo+1, hi+1]
m_shapesModel.endEdit();
```

Contiguous lower:
```cpp
for (int k = lo; k <= hi; ++k) {
    m_shapesModel.moveShape(k, k - 1);
}
// careful: after moveShape(lo, lo-1), indices shift — standard approach:
for (int n = 0; n < block.size(); ++n) {
    m_shapesModel.moveShape(lo + n, lo + n - 1);
}
```
Verify against existing `testZOrder` (~798+) and extend with grouped case.

- [ ] **Step 1: Implement `isContiguous`, `compactSelectionToBlock`, rewrite `raiseSelected`/`lowerSelected`**

- [ ] **Step 2: After groupSelected, call compact so members sit in one z-run**

- [ ] **Step 3: Build + commit**

```bash
cmake --build build --target scribbleway-overlay
git add src/overlay/overlaycontroller.cpp src/overlay/shapesmodel.h src/overlay/shapesmodel.cpp
git commit -m "feat: raise/lower move grouped shapes as one z-order block"
```

---

### Task 4: Excalidraw clipboard `groupIds` + paste remap

**Files:**
- Modify: `src/overlay/overlaycontroller.cpp` — `convertToExcalidraw`, `convertFromExcalidraw`, `pasteFromClipboard`

- [ ] **Step 1: Export**

In `convertToExcalidraw` after `locked` insert (~1133):
```cpp
    const QString groupId = shape.value(QStringLiteral("groupId")).toString();
    if (!groupId.isEmpty()) {
        elem.insert(QStringLiteral("groupIds"), QJsonArray{ groupId });
    } else {
        elem.insert(QStringLiteral("groupIds"), QJsonArray{});
    }
```

- [ ] **Step 2: Import**

In `convertFromExcalidraw` after seed (~1255):
```cpp
    QString groupId;
    const QJsonArray gids = elem.value(QStringLiteral("groupIds")).toArray();
    if (!gids.isEmpty())
        groupId = gids.at(0).toString();
    shape.insert(QStringLiteral("groupId"), groupId);
```

- [ ] **Step 3: Paste id remap**

In `pasteFromClipboard`, before adding shapes (~1056), build `QHash<QString,QString> groupIdRemap`. For each shape to paste:
```cpp
        const QString oldGid = shape.value(QStringLiteral("groupId")).toString();
        if (!oldGid.isEmpty()) {
            if (!groupIdRemap.contains(oldGid))
                groupIdRemap.insert(oldGid, QUuid::createUuid().toString(QUuid::WithoutBraces));
            shape.insert(QStringLiteral("groupId"), groupIdRemap.value(oldGid));
        }
```
Apply after geometry offset, before `addShape`.

- [ ] **Step 4: Commit**

```bash
git add src/overlay/overlaycontroller.cpp
git commit -m "feat: serialize groupIds for Excalidraw clipboard round-trip"
```

---

### Task 5: Shortcuts + applet UI

**Files:**
- Modify: `src/overlay/overlaycontroller.cpp` (`m_localShortcuts` list ~39–61)
- Modify: `src/overlay/qml/main.qml` (Shortcut block ~599+)
- Modify: `src/applet-plugin/appletbackend.h`
- Modify: `src/applet-plugin/appletbackend.cpp`
- Modify: `applet/contents/ui/FullRepresentation.qml`

- [ ] **Step 1: Local shortcut defs**

Append to `m_localShortcuts`:
```cpp
        {QStringLiteral("action_group"), QStringLiteral("Group"), QStringLiteral("Ctrl+G"), QStringLiteral("Ctrl+G")},
        {QStringLiteral("action_ungroup"), QStringLiteral("Ungroup"), QStringLiteral("Ctrl+Shift+G"), QStringLiteral("Ctrl+Shift+G")}
```

- [ ] **Step 2: QML shortcuts in `main.qml`**

After undo/clear shortcuts (~664–673):
```qml
    Shortcut {
        sequence: controller.localShortcutSequences["action_group"]
        enabled: canvasWindow.shortcutGuard
        onActivated: controller.groupSelected()
    }
    Shortcut {
        sequence: controller.localShortcutSequences["action_ungroup"]
        enabled: canvasWindow.shortcutGuard
        onActivated: controller.ungroupSelected()
    }
```

- [ ] **Step 3: AppletBackend**

`appletbackend.h` near raise/lower:
```cpp
    Q_INVOKABLE void groupSelected();
    Q_INVOKABLE void ungroupSelected();
```

`appletbackend.cpp`:
```cpp
void AppletBackend::groupSelected()
{
    sendDBus(QStringLiteral("groupSelected"));
}

void AppletBackend::ungroupSelected()
{
    sendDBus(QStringLiteral("ungroupSelected"));
}
```

D-Bus: slots on `OverlayController` are already exposed via `Q_CLASSINFO` interface when registered as adaptors/slots — confirm `groupSelected`/`ungroupSelected` are in `public Q_SLOTS` (same as `raiseSelected`) so `sendDBus` works without XML changes (project uses introspection on QObject slots).

- [ ] **Step 4: FullRepresentation buttons**

In selection `RowLayout` (~166–188), add:
```qml
            PlasmaComponents.Button {
                icon.name: "object-group"
                text: "Group"
                Layout.fillWidth: true
                enabled: root.backend.hasSelection
                onClicked: root.backend.groupSelected()
            }
            PlasmaComponents.Button {
                icon.name: "object-ungroup"
                text: "Ungroup"
                Layout.fillWidth: true
                enabled: root.backend.hasSelection
                onClicked: root.backend.ungroupSelected()
            }
```
If `canGroup`/`canUngroup` are plumbed through `AppletBackend` selection state, bind `enabled` to those instead.

- [ ] **Step 5: Build + commit**

```bash
cmake --build build --target scribbleway-overlay scribblewaybackend
git add src/overlay/overlaycontroller.cpp src/overlay/qml/main.qml \
  src/applet-plugin/appletbackend.h src/applet-plugin/appletbackend.cpp \
  applet/contents/ui/FullRepresentation.qml
git commit -m "feat: group/ungroup shortcuts and applet actions"
```

---

### Task 6: Tests

**Files:**
- Modify: `tests/shapesmodeltest.cpp`

- [ ] **Step 1: Declare slot**

In class private slots list (~50):
```cpp
    void testGroupUngroup();
```

- [ ] **Step 2: Implement `testGroupUngroup`**

Cover:

1. **Role round-trip:** `updateShape(0, {{"groupId","abc"}})` → `data(index, GroupIdRole) == "abc"`.
2. **groupSelected assigns shared id:** add 3 rects, select 0+1 via `selectShape`, `groupSelected()`, both share non-empty equal `groupId`, shape 2 empty; `canUngroup==true`, `canGroup==false` if only one logical selection of 2… (still 2 selected → canGroup still true if ≥2 selected — either definition OK; assert documented choice: `canGroup` is pure selected-count ≥2).
3. **Selection expansion:** clear selection, `selectShape(0, false)` → shape 1 also `selected==true`.
4. **Drag moves whole group:** `beginEdit`; `dragSelected(10,5)`; both members offset; ungrouped third unchanged.
5. **deleteSelected removes all members** when one selected (after expand) or when group selected.
6. **ungroupSelected clears ids;** subsequent select no longer expands.
7. **Undo restores groupId** after groupSelected.
8. **raiseSelected on grouped pair** keeps relative order and moves both above neighbor (`testZOrder` style).
9. **Clipboard:** group two shapes, `copySelected`, clear, `pasteFromClipboard`; pasted pair shares a **new** `groupId` distinct from any pre-clear id; `groupIds` present in clipboard JSON (optional parse).
10. **Merge groups:** group A=(0,1), group B=(2,3); select all four; `groupSelected()` → one shared id.
11. **Locked members:** if any group member locked, select expand still selects them but drag skips locked via BaseShape `enabled: !isLocked` — controller `dragSelected` currently moves all in `m_dragStartShapes` without locked check. **v1:** when building `m_dragStartShapes` in `beginEdit`, skip `locked==true` shapes (small fix, do in Task 2/3 if not already). Assert locked member not moved.

Sketch:
```cpp
void ShapesModelTest::testGroupUngroup()
{
    OverlayController controller;

    auto makeRect = [](double x, double y) {
        QVariantMap s;
        s[QStringLiteral("type")] = QStringLiteral("rectangle");
        s[QStringLiteral("x")] = x;
        s[QStringLiteral("y")] = y;
        s[QStringLiteral("width")] = 40.0;
        s[QStringLiteral("height")] = 40.0;
        s[QStringLiteral("color")] = QStringLiteral("#ff0000");
        return s;
    };

    controller.addShape(makeRect(0, 0));
    controller.addShape(makeRect(100, 0));
    controller.addShape(makeRect(200, 0));

    controller.selectShape(0, false);
    controller.selectShape(1, true);
    QCOMPARE(controller.hasMultiSelection(), true);

    controller.groupSelected();
    const QString gid = controller.shapesModel()->shapes().at(0).value(QStringLiteral("groupId")).toString();
    QVERIFY(!gid.isEmpty());
    QCOMPARE(controller.shapesModel()->shapes().at(1).value(QStringLiteral("groupId")).toString(), gid);
    QCOMPARE(controller.shapesModel()->shapes().at(2).value(QStringLiteral("groupId")).toString(), QString());

    controller.selectShape(2, false);
    controller.selectShape(0, false); // expand
    QCOMPARE(controller.shapesModel()->shapes().at(0).value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(1).value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(2).value(QStringLiteral("selected")).toBool(), false);

    controller.beginEdit();
    controller.dragSelected(10.0, 5.0);
    controller.endEdit();
    QCOMPARE(controller.shapesModel()->shapes().at(0).value(QStringLiteral("x")).toDouble(), 10.0);
    QCOMPARE(controller.shapesModel()->shapes().at(1).value(QStringLiteral("x")).toDouble(), 110.0);
    QCOMPARE(controller.shapesModel()->shapes().at(2).value(QStringLiteral("x")).toDouble(), 200.0);

    controller.ungroupSelected();
    QCOMPARE(controller.shapesModel()->shapes().at(0).value(QStringLiteral("groupId")).toString(), QString());
    QCOMPARE(controller.shapesModel()->shapes().at(1).value(QStringLiteral("groupId")).toString(), QString());

    // re-group and undo
    controller.selectShape(0, false);
    controller.selectShape(1, true);
    controller.groupSelected();
    QVERIFY(!controller.shapesModel()->shapes().at(0).value(QStringLiteral("groupId")).toString().isEmpty());
    controller.undo();
    QCOMPARE(controller.shapesModel()->shapes().at(0).value(QStringLiteral("groupId")).toString(), QString());
}
```
Extend with z-order + copy/paste cases in the same function or `testGroupZOrder` / `testGroupClipboard` if the function grows past ~150 lines.

- [ ] **Step 3: Run tests**

```bash
cmake --build build --target shapesmodeltest
ctest --test-dir build -R shapesmodeltest --output-on-failure
# or:
./build/bin/shapesmodeltest testGroupUngroup
```
Expected: PASS (and existing `testMultiSelection`, `testZOrder`, `testOverlayControllerCopyPaste` still PASS).

- [ ] **Step 4: Commit**

```bash
git add tests/shapesmodeltest.cpp
git commit -m "test: cover group/ungroup selection, drag, undo, clipboard"
```

---

## Test Plan (manual)

1. Draw three rectangles; Shift-select two; Ctrl+G. Deselect (click empty). Click one member → both selected.
2. Drag → both move. Delete → both removed. Undo → both restored still grouped.
3. Ctrl+Shift+G → ungroup; click one → only one selected.
4. Group two pairs separately; Shift-select one from each; Ctrl+G → one merged group.
5. Raise/Lower with group selected → stack order of both members moves together; relative order preserved.
6. Copy grouped shapes, paste → pasted pair moves as a group; original group still independent.
7. Paste Excalidraw clipboard JSON that includes `"groupIds":["xyz"]` on two elements → they group in Scribbleway.
8. Applet: Group/Ungroup buttons invoke same behavior over D-Bus.
9. Shortcuts appear in applet shortcut editor as local actions.

## Acceptance Criteria

- [ ] Shapes store persistent `groupId`; model exposes `GroupIdRole` / `"groupId"`.
- [ ] `groupSelected()` requires ≥2 selected shapes; assigns one new UUID; merges pre-existing groups in the selection; single undo step.
- [ ] `ungroupSelected()` clears `groupId` on all members of groups intersecting the selection; single undo step.
- [ ] Plain click on a grouped shape selects every member; Shift-click remains single-shape toggle.
- [ ] Drag, delete, lock toggle, and property updates apply to the full active selection (hence full group after expand).
- [ ] Raise/lower move all selected group members as one z-order block without scrambling relative order.
- [ ] Ctrl+G / Ctrl+Shift+G local shortcuts work in select mode (`shortcutGuard`).
- [ ] Applet exposes Group/Ungroup actions.
- [ ] Excalidraw clipboard round-trip preserves grouping via `groupIds`; paste remaps ids.
- [ ] `testGroupUngroup` (and existing suite) passes.
- [ ] No nested group shape type; no QML Repeater hierarchy change.

## Out of Scope (v1)

- Nested groups / multiple simultaneous `groupIds` membership
- Group-level resize (one bbox handle for whole group)
- Double-click to enter “edit group” isolation mode
- Auto-expand marquee selection to full groups
- Visual group hull / dashed collective bounds
- Persisted scene files (no save format yet — when save/load lands, include `groupId`)

## Implementation Notes for Agents

- Match existing edit-transaction style: always `beginEdit` before multi-shape mutations, `endEdit` after, then `notifyShapesChanged` + `notifySelectionChanged`.
- `updateShape` with only `selected` changes does not push history — grouping must change `groupId` (real key) or rely on `beginEdit` snapshot.
- Do not break `testMultiSelection` assumptions: ungrouped multi-select drag/delete stay as today.
- When compacting z-order on group, preserve ascending index order of members (painter’s order).
- Prefer boring helpers on `OverlayController` over new types/classes.
