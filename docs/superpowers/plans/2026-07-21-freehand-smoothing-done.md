# Freehand Smoothing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Smooth freehand polylines on finalize with an optional 0–3 smoothing level so strokes look clean without losing the option for a raw jittery look.

**Architecture:** Keep recording raw mouse points during the gesture (preview stays snappy). On `finalizeShape()`, run a pure-JS pipeline in `RoughPathGenerator.js`: drop near-duplicates → Ramer–Douglas–Peucker simplification → Chaikin corner-cutting. The result replaces `shape.points` before `controller.addShape()`. Level is a draw-time default (`defaultFreehandSmoothing`), not a per-shape re-render property — committed points are the source of truth for rendering, undo, and Excalidraw.

**Tech Stack:** QML / JavaScript (`.pragma library`), Qt 6 C++ (`OverlayController`, `AppletBackend`), D-Bus `updateProperties` / `getSelectionState`, Qt Test + `QJSEngine`.

## Global Constraints

- Smoothing runs **only on finalize**, never during `onPositionChanged` preview.
- Level `0` MUST leave points unchanged (raw look).
- Endpoints MUST be preserved exactly (first and last point).
- Do **not** add a `ShapesModel` role or store `freehandSmoothing` on each shape — only the draw default lives on the controller.
- Excalidraw export/import stays point-list based; no new Excalidraw field.
- Reuse the existing JS test harness pattern in `tests/shapesmodeltest.cpp::testRoughPathGenerator` (`QJSEngine` + `Qt.point` mock).
- Do not change `FreehandShape.qml` rendering contracts or `getSketchyFreehand` roughness behavior.

---

## File Structure & Decomposition

| File | Responsibility |
|------|----------------|
| `src/overlay/qml/shapes/RoughPathGenerator.js` | Pure functions: RDP, Chaikin, level pipeline |
| `src/overlay/qml/main.qml` | Import generator; call smooth in `finalizeShape()` freehand branch |
| `src/overlay/overlaycontroller.h` / `.cpp` | `defaultFreehandSmoothing` Q_PROPERTY, selection state, `updateProperties` |
| `src/applet-plugin/appletbackend.h` / `.cpp` | Expose level to applet; D-Bus set |
| `applet/contents/ui/FullRepresentation.qml` | Smoothing slider (0–3) |
| `tests/shapesmodeltest.cpp` | JS algorithm tests + controller default tests |

---

## Design Detail

### Smoothing levels

| Level | Behavior |
|-------|----------|
| `0` | Identity — return a shallow copy of input points |
| `1` | Dedup near-identical points + RDP with `ε = 1.5` |
| `2` | Dedup + RDP `ε = 2.5` + **1** Chaikin iteration |
| `3` | Dedup + RDP `ε = 4.0` + **2** Chaikin iterations |

Clamp any out-of-range level into `0..3`.

### Pipeline (`smoothFreehandPoints(points, level)`)

```
points
  → normalize to {x,y} / Qt.point
  → if length < 3 or level === 0: return copy
  → strip consecutive points with distance² < 0.25 (0.5px)
  → if still < 3: return endpoints only (or remaining)
  → rdpSimplify(pts, epsilonForLevel)
  → chaikinSmooth(pts, iterationsForLevel)   // 0 iters at level 1
  → force pts[0] = original first, pts[last] = original last
  → return pts
```

### Why RDP + Chaikin (not only moving average)

- **RDP** reduces dense mouse samples and removes micro-wobble while keeping corners the user intended (good for Excalidraw point count and rough freehand Catmull-Rom cost).
- **Chaikin** cuts corners for visual smoothness without inventing a heavy spline fit; works on open polylines and keeps endpoint control simple.
- Rough freehand already runs Catmull-Rom in `getSketchyFreehand` — smoothing the stored polyline improves both `roughness === 0` `PathPolyline` and the rough path inputs.

### Data / API surface

**New controller default (not a shape key):**

- `Q_PROPERTY(int defaultFreehandSmoothing READ defaultFreehandSmoothing WRITE setDefaultFreehandSmoothing NOTIFY defaultFreehandSmoothingChanged)`
- Member: `int m_defaultFreehandSmoothing = 2;` (noticeable but not extreme default)
- `getSelectionState()` always includes `"freehandSmoothing": m_defaultFreehandSmoothing` (both selected and unselected branches)
- `updateProperties`: if key `"freehandSmoothing"` present → `setDefaultFreehandSmoothing(clamped)`
- Selecting a shape does **not** overwrite this default from shape data (shapes have no such key)

**Applet:**

- `Q_PROPERTY(int selectedFreehandSmoothing READ selectedFreehandSmoothing NOTIFY selectionChanged)` — name mirrors glow/borderRadius even though it is a default, not selection-local
- `Q_INVOKABLE void setFreehandSmoothing(int level)` → D-Bus `updateProperties({freehandSmoothing: level})`
- Slider visible when freehand tool is active **or** always under properties (prefer always-visible next to Glow, label “Smoothing”)

**Finalize (`main.qml` ~132–137):**

```javascript
if (activePoints.length >= 2) {
    shape["points"] = RoughPathGenerator.smoothFreehandPoints(
        activePoints, controller.defaultFreehandSmoothing);
    controller.addShape(shape);
}
```

### Excalidraw compatibility impact

- **None on schema.** Freehand still exports as `freedraw` with relative `points` arrays (`overlaycontroller.cpp` ~1177–1222).
- Smoothed strokes typically export **fewer** points (RDP) — desirable.
- Paste from Excalidraw still loads raw point lists unchanged (no re-smoothing on paste).
- No new QVariantMap keys on stored shapes; no new ShapeRoles.

---

### Task 1: Smoothing algorithms in RoughPathGenerator.js

**Files:**
- Modify: `src/overlay/qml/shapes/RoughPathGenerator.js` (append after `getSketchyFreehand`, ~line 325)
- Test: `tests/shapesmodeltest.cpp` (extend in Task 4; implement algorithms first)

**Interfaces:**
- Consumes: existing `Qt.point` usage in the module
- Produces:
  - `function pointDistSq(a, b) → number`
  - `function dedupPoints(points, minDistSq) → Array`
  - `function rdpSimplify(points, epsilon) → Array`
  - `function chaikinSmooth(points, iterations) → Array`
  - `function smoothFreehandPoints(points, level) → Array`

- [ ] **Step 1: Append algorithm helpers**

Add to the end of `src/overlay/qml/shapes/RoughPathGenerator.js`:

```javascript
function pointDistSq(a, b) {
    let dx = a.x - b.x;
    let dy = a.y - b.y;
    return dx * dx + dy * dy;
}

function copyPoint(p) {
    return Qt.point(p.x, p.y);
}

function copyPoints(points) {
    let out = [];
    for (let i = 0; i < points.length; ++i) {
        out.push(copyPoint(points[i]));
    }
    return out;
}

// Drop consecutive samples closer than sqrt(minDistSq).
function dedupPoints(points, minDistSq) {
    if (!points || points.length === 0) return [];
    let out = [copyPoint(points[0])];
    for (let i = 1; i < points.length; ++i) {
        if (pointDistSq(out[out.length - 1], points[i]) >= minDistSq) {
            out.push(copyPoint(points[i]));
        }
    }
    // Always keep the true endpoint if it was collapsed away
    let last = points[points.length - 1];
    if (out.length === 1 || pointDistSq(out[out.length - 1], last) > 1e-12) {
        if (pointDistSq(out[out.length - 1], last) >= minDistSq || out.length === 1) {
            // if last equals only point, still fine; if collapsed, force endpoint
            if (pointDistSq(out[out.length - 1], last) > 1e-12) {
                out.push(copyPoint(last));
            }
        } else {
            out[out.length - 1] = copyPoint(last);
        }
    }
    return out;
}

function rdpPerpendicularDistSq(p, a, b) {
    let dx = b.x - a.x;
    let dy = b.y - a.y;
    let lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-12) {
        return pointDistSq(p, a);
    }
    // Area formula: |(p-a) × (b-a)| / |b-a|  → squared
    let cross = (p.x - a.x) * dy - (p.y - a.y) * dx;
    return (cross * cross) / lenSq;
}

// Classic Ramer–Douglas–Peucker. points length >= 2. epsilon in pixels.
function rdpSimplify(points, epsilon) {
    if (!points || points.length < 3) {
        return copyPoints(points || []);
    }
    let epsSq = epsilon * epsilon;
    let keep = [];
    for (let i = 0; i < points.length; ++i) {
        keep.push(false);
    }
    keep[0] = true;
    keep[points.length - 1] = true;

    let stack = [[0, points.length - 1]];
    while (stack.length > 0) {
        let seg = stack.pop();
        let start = seg[0];
        let end = seg[1];
        let maxDist = -1;
        let index = -1;
        for (let i = start + 1; i < end; ++i) {
            let d = rdpPerpendicularDistSq(points[i], points[start], points[end]);
            if (d > maxDist) {
                maxDist = d;
                index = i;
            }
        }
        if (index >= 0 && maxDist > epsSq) {
            keep[index] = true;
            stack.push([start, index]);
            stack.push([index, end]);
        }
    }

    let out = [];
    for (let i = 0; i < points.length; ++i) {
        if (keep[i]) {
            out.push(copyPoint(points[i]));
        }
    }
    return out;
}

// Chaikin corner-cutting on an open polyline. Endpoints stay fixed.
// One iteration: for each segment (P_i, P_{i+1}) emit Q=0.75P_i+0.25P_{i+1}, R=0.25P_i+0.75P_{i+1}
// then [P0, Q0, R0, Q1, R1, ..., Pn].
function chaikinSmooth(points, iterations) {
    if (!points || points.length < 3 || iterations <= 0) {
        return copyPoints(points || []);
    }
    let pts = copyPoints(points);
    for (let iter = 0; iter < iterations; ++iter) {
        if (pts.length < 3) break;
        let next = [];
        next.push(copyPoint(pts[0]));
        for (let i = 0; i < pts.length - 1; ++i) {
            let p0 = pts[i];
            let p1 = pts[i + 1];
            next.push(Qt.point(
                0.75 * p0.x + 0.25 * p1.x,
                0.75 * p0.y + 0.25 * p1.y
            ));
            next.push(Qt.point(
                0.25 * p0.x + 0.75 * p1.x,
                0.25 * p0.y + 0.75 * p1.y
            ));
        }
        next.push(copyPoint(pts[pts.length - 1]));
        pts = next;
    }
    return pts;
}

function smoothFreehandLevelParams(level) {
    let lv = level | 0;
    if (lv < 0) lv = 0;
    if (lv > 3) lv = 3;
    // epsilon px, chaikin iterations
    if (lv === 0) return { epsilon: 0, iterations: 0 };
    if (lv === 1) return { epsilon: 1.5, iterations: 0 };
    if (lv === 2) return { epsilon: 2.5, iterations: 1 };
    return { epsilon: 4.0, iterations: 2 };
}

// Public entry: smooth freehand samples for commit.
// points: array of Qt.point / {x,y}. level: 0..3.
function smoothFreehandPoints(points, level) {
    if (!points || points.length === 0) return [];
    if (points.length < 2) return copyPoints(points);

    let params = smoothFreehandLevelParams(level);
    if (params.epsilon === 0 && params.iterations === 0) {
        return copyPoints(points);
    }

    let first = copyPoint(points[0]);
    let last = copyPoint(points[points.length - 1]);

    // 0.5px consecutive dedup
    let pts = dedupPoints(points, 0.25);
    if (pts.length < 3) {
        return [first, last];
    }

    pts = rdpSimplify(pts, params.epsilon);
    if (pts.length < 2) {
        return [first, last];
    }

    pts = chaikinSmooth(pts, params.iterations);

    // Pin endpoints to the original stroke ends
    pts[0] = first;
    pts[pts.length - 1] = last;
    return pts;
}
```

- [ ] **Step 2: Commit algorithms**

```bash
git add src/overlay/qml/shapes/RoughPathGenerator.js
git commit -m "feat: add RDP + Chaikin freehand smoothing helpers"
```

---

### Task 2: Apply smoothing on freehand finalize

**Files:**
- Modify: `src/overlay/qml/main.qml`

**Interfaces:**
- Consumes: `RoughPathGenerator.smoothFreehandPoints`, `controller.defaultFreehandSmoothing` (Task 3 adds the property; until then hardcode `2` only if implementing QML first — prefer Task 3 before or with this task)
- Produces: freehand shapes committed with smoothed `points`

- [ ] **Step 1: Import RoughPathGenerator at top of main.qml**

After existing imports (`src/overlay/qml/main.qml` lines 1–4):

```qml
import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls
import QtQuick.Shapes
import "shapes/RoughPathGenerator.js" as RoughPathGenerator
```

- [ ] **Step 2: Smooth points in finalizeShape freehand branch**

Replace the freehand branch in `finalizeShape()` (`src/overlay/qml/main.qml` ~132–137):

```javascript
        if (activeDrawTool === "freehand") {
            // Only add if we have at least 2 points
            if (activePoints.length >= 2) {
                let level = controller.defaultFreehandSmoothing;
                if (level === undefined || level === null) {
                    level = 2;
                }
                shape["points"] = RoughPathGenerator.smoothFreehandPoints(activePoints, level);
                controller.addShape(shape);
            }
        } else if (activeDrawTool === "line" || activeDrawTool === "arrow") {
```

Do **not** assign smoothed points back into `freehandPreviewPath` after release (preview is cleared via `activePoints = []` at end of `finalizeShape`).

Leave `onPositionChanged` freehand batching (`main.qml` ~265–270) untouched — raw points only.

- [ ] **Step 3: Commit finalize wiring**

```bash
git add src/overlay/qml/main.qml
git commit -m "feat: smooth freehand points on shape finalize"
```

---

### Task 3: Controller default + applet plumbing

**Files:**
- Modify: `src/overlay/overlaycontroller.h`
- Modify: `src/overlay/overlaycontroller.cpp`
- Modify: `src/applet-plugin/appletbackend.h`
- Modify: `src/applet-plugin/appletbackend.cpp`
- Modify: `applet/contents/ui/FullRepresentation.qml`

**Interfaces:**
- Consumes: D-Bus `updateProperties` / `getSelectionState` / `selectionChanged` pattern used by glow
- Produces:
  - `OverlayController::defaultFreehandSmoothing()` / `setDefaultFreehandSmoothing(int)`
  - selection map key `"freehandSmoothing"`
  - `AppletBackend::selectedFreehandSmoothing` / `setFreehandSmoothing(int)`
  - Applet slider

- [ ] **Step 1: Declare property on OverlayController**

In `src/overlay/overlaycontroller.h`:

After `defaultGlow` property (~line 44):

```cpp
    Q_PROPERTY(int defaultGlow READ defaultGlow WRITE setDefaultGlow NOTIFY defaultGlowChanged)
    Q_PROPERTY(int defaultFreehandSmoothing READ defaultFreehandSmoothing WRITE setDefaultFreehandSmoothing NOTIFY defaultFreehandSmoothingChanged)
```

Public API next to glow getters/setters (~line 80):

```cpp
    int defaultGlow() const;
    void setDefaultGlow(int glow);
    int defaultFreehandSmoothing() const;
    void setDefaultFreehandSmoothing(int level);
    void setDefaultBorderRadius(int radius);
```

Signal (~line 147):

```cpp
    void defaultGlowChanged();
    void defaultFreehandSmoothingChanged();
```

Member (~line 186):

```cpp
    int m_defaultGlow = 10;
    int m_defaultFreehandSmoothing = 2;
```

- [ ] **Step 2: Implement controller getter/setter and wire state**

In `src/overlay/overlaycontroller.cpp`, after `setDefaultGlow`:

```cpp
int OverlayController::defaultFreehandSmoothing() const
{
    return m_defaultFreehandSmoothing;
}

void OverlayController::setDefaultFreehandSmoothing(int level)
{
    int clamped = qBound(0, level, 3);
    if (m_defaultFreehandSmoothing != clamped) {
        m_defaultFreehandSmoothing = clamped;
        Q_EMIT defaultFreehandSmoothingChanged();
    }
}
```

In `getSelectionState()` selected branch after glow (~388):

```cpp
        state[QStringLiteral("glow")] = shape.value(QStringLiteral("glow"), m_defaultGlow).toInt();
        state[QStringLiteral("freehandSmoothing")] = m_defaultFreehandSmoothing;
        state[QStringLiteral("seed")] = shape.value(QStringLiteral("seed"), 123456).toInt();
```

In the no-selection branch after glow (~402):

```cpp
        state[QStringLiteral("glow")] = m_defaultGlow;
        state[QStringLiteral("freehandSmoothing")] = m_defaultFreehandSmoothing;
        state[QStringLiteral("locked")] = false;
```

In `updateProperties()` after glow handling (read current glow block ~434+ and mirror):

```cpp
    if (demarshalled.contains(QStringLiteral("freehandSmoothing"))) {
        setDefaultFreehandSmoothing(demarshalled[QStringLiteral("freehandSmoothing")].toInt());
    }
```

**Do not** copy freehandSmoothing from selected shapes in `setSelectedIndex` — shapes do not carry it.

After mutating the default via `updateProperties`, existing code already calls `notifySelectionChanged()` at the end of `updateProperties` (verify and keep that path so the applet slider updates).

- [ ] **Step 3: AppletBackend property + D-Bus set**

`src/applet-plugin/appletbackend.h` — property near glow:

```cpp
    Q_PROPERTY(int selectedGlow READ selectedGlow NOTIFY selectionChanged)
    Q_PROPERTY(int selectedFreehandSmoothing READ selectedFreehandSmoothing NOTIFY selectionChanged)
```

Getter + invokable:

```cpp
    int selectedGlow() const;
    int selectedFreehandSmoothing() const;
```

```cpp
    Q_INVOKABLE void setGlow(int glow);
    Q_INVOKABLE void setFreehandSmoothing(int level);
```

Member:

```cpp
    int m_selectedGlow = 10;
    int m_selectedFreehandSmoothing = 2;
```

`src/applet-plugin/appletbackend.cpp`:

```cpp
int AppletBackend::selectedFreehandSmoothing() const
{
    return m_selectedFreehandSmoothing;
}

void AppletBackend::setFreehandSmoothing(int level)
{
    sendDBus(QStringLiteral("updateProperties"),
             { QVariantMap{{QStringLiteral("freehandSmoothing"), level}} });
}
```

In `onSelectionChanged` after glow parse (~362):

```cpp
    m_selectedGlow = demarshalled.value(QStringLiteral("glow"), 3).toInt();
    m_selectedFreehandSmoothing = demarshalled.value(QStringLiteral("freehandSmoothing"), 2).toInt();
    Q_EMIT selectionChanged();
```

- [ ] **Step 4: Applet slider UI**

In `applet/contents/ui/FullRepresentation.qml`, add a row after the Glow slider block (~lines 343–367), matching its layout:

```qml
        // Freehand smoothing (draw-time default; 0 = raw)
        RowLayout {
            Layout.fillWidth: true
            PlasmaComponents.Label {
                text: "Smoothing"
                Layout.preferredWidth: Kirigami.Units.gridUnit * 5
            }
            PlasmaComponents.Slider {
                Layout.fillWidth: true
                from: 0
                to: 3
                stepSize: 1
                value: root.backend.selectedFreehandSmoothing
                onMoved: {
                    root.backend.setFreehandSmoothing(value)
                }
            }
            PlasmaComponents.Label {
                text: {
                    let v = Math.round(root.backend.selectedFreehandSmoothing)
                    if (v <= 0) return "Off"
                    if (v === 1) return "Low"
                    if (v === 2) return "Med"
                    return "High"
                }
            }
        }
```

Mirror the surrounding RowLayout/Label structure already used for Stroke / Opacity / Glow so spacing stays consistent (copy the Glow block’s exact layout chrome if it differs slightly from the sketch above).

- [ ] **Step 5: Build check**

```bash
cmake --build build --target scribbleway-overlay scribblewaybackend
```

Expected: compiles cleanly.

- [ ] **Step 6: Commit**

```bash
git add \
  src/overlay/overlaycontroller.h \
  src/overlay/overlaycontroller.cpp \
  src/applet-plugin/appletbackend.h \
  src/applet-plugin/appletbackend.cpp \
  applet/contents/ui/FullRepresentation.qml
git commit -m "feat: expose freehand smoothing level via controller and applet"
```

---

### Task 4: Tests

**Files:**
- Modify: `tests/shapesmodeltest.cpp`

**Interfaces:**
- Consumes: `smoothFreehandPoints`, `rdpSimplify`, `chaikinSmooth`, `OverlayController::defaultFreehandSmoothing`
- Produces: `testFreehandSmoothing()` + extensions inside `testRoughPathGenerator()` or a dedicated JS section

- [ ] **Step 1: Declare test slots**

In the private slots section of `ShapesModelTest` (~line 39–50):

```cpp
    void testGlow();
    void testFreehandSmoothing();
    void testExcalidrawPasteCompatibility();
    // ...
    void testRoughPathGenerator();
```

- [ ] **Step 2: Controller default test**

Add after `testGlow()`:

```cpp
void ShapesModelTest::testFreehandSmoothing()
{
    OverlayController controller;

    // Default level is medium (2)
    QCOMPARE(controller.defaultFreehandSmoothing(), 2);

    QVariantMap updateProps;
    updateProps[QStringLiteral("freehandSmoothing")] = 0;
    controller.updateProperties(updateProps);
    QCOMPARE(controller.defaultFreehandSmoothing(), 0);

    // Clamp high
    controller.updateProperties({{QStringLiteral("freehandSmoothing"), 99}});
    QCOMPARE(controller.defaultFreehandSmoothing(), 3);

    // Clamp low
    controller.updateProperties({{QStringLiteral("freehandSmoothing"), -5}});
    QCOMPARE(controller.defaultFreehandSmoothing(), 0);

    // Selection state always reports the draw default
    QVariantMap state = controller.getSelectionState();
    QCOMPARE(state.value(QStringLiteral("freehandSmoothing")).toInt(), 0);

    // Freehand shape does not need freehandSmoothing key; state still reports default
    QVariantMap freehand;
    freehand[QStringLiteral("type")] = QStringLiteral("freehand");
    QVariantList pts;
    pts.append(QPointF(0.0, 0.0));
    pts.append(QPointF(10.0, 0.0));
    pts.append(QPointF(10.0, 10.0));
    freehand[QStringLiteral("points")] = pts;
    controller.addShape(freehand);

    state = controller.getSelectionState();
    QVERIFY(state.value(QStringLiteral("hasSelection")).toBool());
    QCOMPARE(state.value(QStringLiteral("freehandSmoothing")).toInt(), 0);
    // Shape map must not be required to contain freehandSmoothing
    QVERIFY(!controller.shapesModel()->shapes().first().contains(QStringLiteral("freehandSmoothing")));
}
```

- [ ] **Step 3: JS algorithm assertions in testRoughPathGenerator**

At the end of `testRoughPathGenerator()` (before the closing `}`), after freehand sketchy tests, add:

```cpp
    // --- Freehand smoothing helpers ---
    QJSValue smoothFreehandPoints = engine.globalObject().property(QStringLiteral("smoothFreehandPoints"));
    QVERIFY(smoothFreehandPoints.isCallable());
    QJSValue rdpSimplify = engine.globalObject().property(QStringLiteral("rdpSimplify"));
    QVERIFY(rdpSimplify.isCallable());
    QJSValue chaikinSmooth = engine.globalObject().property(QStringLiteral("chaikinSmooth"));
    QVERIFY(chaikinSmooth.isCallable());

    auto makePt = [&](double x, double y) {
        QJSValue o = engine.newObject();
        o.setProperty(QStringLiteral("x"), x);
        o.setProperty(QStringLiteral("y"), y);
        return o;
    };
    auto makePts = [&](std::initializer_list<std::pair<double, double>> coords) {
        QJSValue arr = engine.newArray();
        int i = 0;
        for (const auto &c : coords) {
            arr.setProperty(i++, makePt(c.first, c.second));
        }
        return arr;
    };
    auto lenOf = [](const QJSValue &v) {
        return v.property(QStringLiteral("length")).toInt();
    };
    auto xAt = [](const QJSValue &arr, int i) {
        return arr.property(i).property(QStringLiteral("x")).toNumber();
    };
    auto yAt = [](const QJSValue &arr, int i) {
        return arr.property(i).property(QStringLiteral("y")).toNumber();
    };

    // Colinear dense samples → RDP collapses to endpoints
    QJSValue dense = makePts({
        {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}
    });
    QJSValue rdpOut = rdpSimplify.call(QJSValueList() << dense << 0.5);
    QVERIFY(!rdpOut.isError());
    QCOMPARE(lenOf(rdpOut), 2);
    QCOMPARE(xAt(rdpOut, 0), 0.0);
    QCOMPARE(yAt(rdpOut, 0), 0.0);
    QCOMPARE(xAt(rdpOut, 1), 5.0);
    QCOMPARE(yAt(rdpOut, 1), 0.0);

    // Corner is kept by RDP
    QJSValue corner = makePts({{0, 0}, {5, 0}, {5, 5}});
    QJSValue rdpCorner = rdpSimplify.call(QJSValueList() << corner << 0.5);
    QCOMPARE(lenOf(rdpCorner), 3);

    // Level 0 is identity (same count, same endpoints)
    QJSValue jagged = makePts({
        {0, 0}, {1, 0.4}, {2, -0.3}, {3, 0.2}, {4, 0}, {5, 1}, {6, 6}
    });
    QJSValue lvl0 = smoothFreehandPoints.call(QJSValueList() << jagged << 0);
    QVERIFY(!lvl0.isError());
    QCOMPARE(lenOf(lvl0), lenOf(jagged));
    QCOMPARE(xAt(lvl0, 0), 0.0);
    QCOMPARE(yAt(lvl0, 0), 0.0);
    QCOMPARE(xAt(lvl0, lenOf(lvl0) - 1), 6.0);
    QCOMPARE(yAt(lvl0, lenOf(lvl0) - 1), 6.0);

    // Level 2/3 keep endpoints, produce finite coords, and are defined for short strokes
    for (int level : {1, 2, 3}) {
        QJSValue smoothed = smoothFreehandPoints.call(QJSValueList() << jagged << level);
        QVERIFY2(!smoothed.isError(), qPrintable(QStringLiteral("level %1 errored").arg(level)));
        QVERIFY(lenOf(smoothed) >= 2);
        QCOMPARE(xAt(smoothed, 0), 0.0);
        QCOMPARE(yAt(smoothed, 0), 0.0);
        QCOMPARE(xAt(smoothed, lenOf(smoothed) - 1), 6.0);
        QCOMPARE(yAt(smoothed, lenOf(smoothed) - 1), 6.0);
        for (int i = 0; i < lenOf(smoothed); ++i) {
            QVERIFY(qIsFinite(xAt(smoothed, i)));
            QVERIFY(qIsFinite(yAt(smoothed, i)));
        }
    }

    // Two-point stroke remains two points at any level
    QJSValue two = makePts({{10, 10}, {20, 30}});
    QJSValue twoSm = smoothFreehandPoints.call(QJSValueList() << two << 3);
    QCOMPARE(lenOf(twoSm), 2);

    // Chaikin one iteration grows point count on a 3-point polyline:
    // endpoints + 2 per segment × 2 segments = 2 + 4 = 6
    QJSValue tri = makePts({{0, 0}, {10, 0}, {10, 10}});
    QJSValue ch = chaikinSmooth.call(QJSValueList() << tri << 1);
    QCOMPARE(lenOf(ch), 6);
    QCOMPARE(xAt(ch, 0), 0.0);
    QCOMPARE(yAt(ch, 0), 0.0);
    QCOMPARE(xAt(ch, 5), 10.0);
    QCOMPARE(yAt(ch, 5), 10.0);
```

- [ ] **Step 4: Run tests**

```bash
cmake --build build --target shapesmodeltest
ctest --test-dir build -R shapesmodeltest --output-on-failure
```

Expected: `testFreehandSmoothing` and `testRoughPathGenerator` PASS.

- [ ] **Step 5: Commit**

```bash
git add tests/shapesmodeltest.cpp
git commit -m "test: cover freehand smoothing algorithms and controller default"
```

---

### Task 5: Manual acceptance smoke

**Files:** none (runtime verification)

- [ ] **Step 1: Run overlay + applet**

```bash
# from build tree / installed package as you normally develop
./build/bin/scribbleway-overlay &   # or project-equivalent target path
```

- [ ] **Step 2: Verify matrix**

| Action | Expected |
|--------|----------|
| Smoothing slider Off (0), draw freehand | Stroke matches raw mouse jitter |
| Slider Med (2), draw freehand | Visibly cleaner curve; corners not erased completely |
| Slider High (3), draw freehand | Softest stroke; endpoints match press/release |
| During drag before release | Preview still shows dense raw polyline |
| After release | Committed shape uses smoothed points; selection bounds hug stroke |
| Roughness > 0 on smoothed freehand | `getSketchyFreehand` still renders two sketchy strokes |
| Undo after freehand | Removes the smoothed shape in one step |
| Copy freehand → paste | Points round-trip; no crash; no `freehandSmoothing` key required |
| Non-freehand tools | Unaffected |

- [ ] **Step 3: Final commit only if smoke prompted fixes**

```bash
git status
# commit any fixups with message explaining the correction
```

---

## Test Plan (summary)

1. **Unit (JS via QJSEngine):** RDP collinear collapse, corner retention, Chaikin growth, level 0 identity, endpoint pinning, short strokes.
2. **Unit (C++):** default `2`, clamp `0..3`, `getSelectionState` key, no shape key pollution.
3. **Manual:** slider levels, preview vs finalize, roughness interaction, undo, Excalidraw copy/paste.

## Excalidraw Compatibility Impact

- **Schema:** none.
- **Behavior:** exported `freedraw.points` are the post-smooth absolute→relative conversion of committed points (`convertToExcalidraw` freehand branch ~1177–1222). Fewer points after RDP is fine and usually better.
- **Import:** `convertFromExcalidraw` freehand branch (~1309–1320) unchanged; pasted strokes are not re-smoothed.
- **Clipboard extraneous keys:** `freehandSmoothing` is controller-only; never written into shape maps or Excalidraw JSON.

## Acceptance Criteria

- [ ] `smoothFreehandPoints` / `rdpSimplify` / `chaikinSmooth` exist in `RoughPathGenerator.js` and are covered by tests.
- [ ] Freehand finalize in `main.qml` commits smoothed points using `controller.defaultFreehandSmoothing`.
- [ ] Level `0` preserves raw geometry; levels `1–3` increase smoothness; endpoints always match gesture start/end.
- [ ] Live freehand preview remains raw samples until mouse release.
- [ ] `defaultFreehandSmoothing` is a controller Q_PROPERTY defaulting to `2`, clamped to `0..3`.
- [ ] Applet shows a 0–3 Smoothing control wired through D-Bus `updateProperties`.
- [ ] No new `ShapesModel` role; no per-shape `freehandSmoothing` key.
- [ ] Existing shape types, roughness freehand, and Excalidraw freehand tests still pass.
- [ ] `ctest -R shapesmodeltest` green.

---

## Self-Review

1. **Spec coverage:** finalize pass ✓, RDP ✓, Chaikin ✓, optional level ✓, raw look at 0 ✓, key files touched ✓.
2. **Placeholders:** none — full function bodies and test code included.
3. **Type consistency:** key name `freehandSmoothing` everywhere; property `defaultFreehandSmoothing` on controller; applet `selectedFreehandSmoothing` / `setFreehandSmoothing` mirrors glow naming.
