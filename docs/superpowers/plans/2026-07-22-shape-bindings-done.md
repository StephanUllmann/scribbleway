# Shape Binding / Connection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Arrows and lines auto-snap their endpoints to the edges of rectangles/ellipses within 20px, creating persistent bindings so that moving/resizing/deleting the anchored shape correctly updates the connected endpoints.

**Architecture:** Shape bindings are stored as optional fields on each line/arrow shape (`startBinding`, `endBinding`) referencing a target shape by stable UUID. Target shapes carry back-references (`boundElementIds`). The binding records a parametric `focus` value (perimeter fraction for rects, angle for ellipses) that lets us recompute the exact attachment point after a move/resize. Snap detection runs in QML at draw-time and endpoint-drag-time. Move/resize/delete propagation runs in C++ in `OverlayController`. Excalidraw's `startBinding`/`endBinding` schema is mapped bidirectionally.

**Tech Stack:** Qt 6 / C++ 20 (ShapesModel, OverlayController), QML/JS (main.qml, BaseShape.qml)

## Global Constraints

- Freehand shapes MUST NOT participate in bindings (neither as connector nor as target)
- Text shapes MUST NOT be binding targets
- Snap threshold: 20px from shape outline
- Auto-snap always active for line/arrow tools — no modifier key required
- Dragging a bound endpoint immediately breaks its binding; re-snap on release if near a target
- Excalidraw clipboard compatibility must be preserved (bindings round-trip)
- Undo/redo must preserve/restore bindings (free: bindings live in QVariantMap shape data stored in history)

---

### Task 1: Stable Shape IDs

Every shape needs a stable identity that survives reordering (raise/lower), insertion, and deletion. Currently shapes are identified only by model index, which shifts. Bindings reference shapes by these IDs.

**Files:**
- Modify: `src/overlay/shapesmodel.h:14-37` (add IdRole to enum)
- Modify: `src/overlay/shapesmodel.cpp:37-97` (data(), roleNames())
- Modify: `src/overlay/shapesmodel.cpp:173-185` (addShape — auto-generate ID)
- Modify: `src/overlay/overlaycontroller.h:28-196` (add indexForId helper)
- Modify: `src/overlay/overlaycontroller.cpp:277-286` (addShape — ensure ID before insert)
- Modify: `src/overlay/overlaycontroller.cpp:1132-1258` (convertToExcalidraw — emit id)
- Modify: `src/overlay/overlaycontroller.cpp:1260-1356` (convertFromExcalidraw — import id)
- Test: `tests/shapesmodeltest.cpp`

**Interfaces:**
- Produces: `ShapesModel::IdRole` enum value, `model.shapeId` in QML, `OverlayController::indexForId(const QString &id) -> int` returning -1 if not found, `ShapesModel::shapeIdAt(int index) -> QString`.

- [ ] **Step 1: Add IdRole to ShapesModel enum and wire data()/roleNames()**

In `shapesmodel.h`, add `IdRole` after `GlowRole`:

```cpp
enum ShapeRoles {
    TypeRole = Qt::UserRole + 1,
    ColorRole,
    StrokeWidthRole,
    OpacityRole,
    SelectedRole,
    LockedRole,
    PointsRole,
    XRole,
    YRole,
    WidthRole,
    HeightRole,
    FromXRole,
    FromYRole,
    ToXRole,
    ToYRole,
    TextRole,
    FontFamilyRole,
    FontSizeRole,
    BorderRadiusRole,
    RoughnessRole,
    SeedRole,
    GlowRole,
    IdRole
};
```

Add a public helper:

```cpp
QString shapeIdAt(int index) const;
```

In `shapesmodel.cpp` `data()`, add case:

```cpp
case IdRole: return shape.value(QStringLiteral("id"));
```

In `roleNames()`, add:

```cpp
roles[IdRole] = "shapeId";
```

In `updateShape()`, add the role mapping line alongside the others:

```cpp
else if (it.key() == QStringLiteral("id")) changedRoles << IdRole;
```

Implement `shapeIdAt`:

```cpp
QString ShapesModel::shapeIdAt(int index) const
{
    if (index >= 0 && index < m_shapes.size()) {
        return m_shapes[index].value(QStringLiteral("id")).toString();
    }
    return QString();
}
```

- [ ] **Step 2: Auto-generate ID on addShape**

In `ShapesModel::addShape()`, before `beginInsertRows`, ensure the shape has an `id`:

```cpp
void ShapesModel::addShape(const QVariantMap &shape)
{
    if (!m_isApplyingUndo) {
        saveHistorySnapshot();
    }
    QVariantMap normalizedShape = shape;
    if (!normalizedShape.contains(QStringLiteral("id")) || normalizedShape.value(QStringLiteral("id")).toString().isEmpty()) {
        normalizedShape.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    if (normalizedShape.contains(QStringLiteral("points"))) {
        normalizedShape.insert(QStringLiteral("points"), normalizePoints(normalizedShape.value(QStringLiteral("points"))));
    }
    beginInsertRows(QModelIndex(), m_shapes.size(), m_shapes.size());
    m_shapes.append(normalizedShape);
    endInsertRows();
}
```

Add `#include <QUuid>` to `shapesmodel.cpp`.

- [ ] **Step 3: Add indexForId to OverlayController**

In `overlaycontroller.h`, add as a public Q_INVOKABLE:

```cpp
Q_INVOKABLE int indexForId(const QString &id) const;
```

In `overlaycontroller.cpp`:

```cpp
int OverlayController::indexForId(const QString &id) const
{
    if (id.isEmpty()) return -1;
    const auto &shapes = m_shapesModel.shapes();
    for (int i = 0; i < shapes.size(); ++i) {
        if (shapes[i].value(QStringLiteral("id")).toString() == id) {
            return i;
        }
    }
    return -1;
}
```

- [ ] **Step 4: Excalidraw conversion — pass through id**

In `convertToExcalidraw()`, replace the UUID generation line (`QString id = QUuid::...`) with:

```cpp
QString id = shape.value(QStringLiteral("id")).toString();
if (id.isEmpty()) {
    id = QUuid::createUuid().toString(QUuid::WithoutBraces).mid(0, 8);
}
elem.insert(QStringLiteral("id"), id);
```

In `convertFromExcalidraw()`, after inserting `seed`, add:

```cpp
QString id = elem.value(QStringLiteral("id")).toString();
if (!id.isEmpty()) {
    shape.insert(QStringLiteral("id"), id);
}
```

- [ ] **Step 5: Write tests for stable IDs**

In `shapesmodeltest.cpp`, add test method declaration in the class and implementation:

```cpp
void ShapesModelTest::testStableShapeIds()
{
    ShapesModel model;

    // Adding a shape auto-generates an ID
    QVariantMap rect;
    rect["type"] = "rectangle";
    rect["x"] = 10.0;
    rect["y"] = 20.0;
    rect["width"] = 100.0;
    rect["height"] = 50.0;
    model.addShape(rect);

    QString id1 = model.shapeIdAt(0);
    QVERIFY(!id1.isEmpty());

    // Adding another shape gets a different ID
    QVariantMap rect2;
    rect2["type"] = "ellipse";
    rect2["x"] = 200.0;
    rect2["y"] = 200.0;
    rect2["width"] = 80.0;
    rect2["height"] = 80.0;
    model.addShape(rect2);

    QString id2 = model.shapeIdAt(1);
    QVERIFY(!id2.isEmpty());
    QVERIFY(id1 != id2);

    // Shape with existing ID keeps it
    QVariantMap rect3;
    rect3["type"] = "rectangle";
    rect3["id"] = "my-custom-id";
    rect3["x"] = 50.0;
    rect3["y"] = 50.0;
    rect3["width"] = 30.0;
    rect3["height"] = 30.0;
    model.addShape(rect3);
    QCOMPARE(model.shapeIdAt(2), QStringLiteral("my-custom-id"));

    // ID survives reorder (moveShape)
    model.moveShape(0, 1);
    // After move: index 0 = rect2 (id2), index 1 = rect (id1), index 2 = rect3
    QCOMPARE(model.shapeIdAt(0), id2);
    QCOMPARE(model.shapeIdAt(1), id1);
}
```

Add `testStableShapeIds` to the private Q_SLOTS list in the test class.

- [ ] **Step 6: Build and run tests**

Run: `cd ~/coding/projects/scribbleway/build-deb && cmake .. && make -j$(nproc) && ctest --output-on-failure`

Expected: All existing tests pass, new `testStableShapeIds` passes.

- [ ] **Step 7: Commit**

```bash
git add src/overlay/shapesmodel.h src/overlay/shapesmodel.cpp src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp tests/shapesmodeltest.cpp
git commit -m "feat: add stable shape IDs for binding references"
```

---

### Task 2: Binding Data Model

Add the binding and back-reference fields to the model. Bindings are stored as `QVariantMap` on line/arrow shapes; back-references as `QVariantList` on rect/ellipse shapes.

**Files:**
- Modify: `src/overlay/shapesmodel.h:14-37` (add roles)
- Modify: `src/overlay/shapesmodel.cpp:37-97` (data(), roleNames(), updateShape role mapping)
- Test: `tests/shapesmodeltest.cpp`

**Interfaces:**
- Consumes: `IdRole` from Task 1
- Produces: `StartBindingRole`, `EndBindingRole`, `BoundElementIdsRole` model roles; QML properties `model.startBinding`, `model.endBinding`, `model.boundElementIds`

A binding QVariantMap has this shape:
```
{ "elementId": "<uuid-string>", "focus": <double 0.0-1.0>, "gap": <double> }
```

- [ ] **Step 1: Add roles to ShapesModel enum**

In `shapesmodel.h`, extend the enum after `IdRole`:

```cpp
    IdRole,
    StartBindingRole,
    EndBindingRole,
    BoundElementIdsRole
```

- [ ] **Step 2: Wire data(), roleNames(), updateShape()**

In `data()`, add cases:

```cpp
case StartBindingRole: return shape.value(QStringLiteral("startBinding"));
case EndBindingRole: return shape.value(QStringLiteral("endBinding"));
case BoundElementIdsRole: return shape.value(QStringLiteral("boundElementIds"));
```

In `roleNames()`:

```cpp
roles[StartBindingRole] = "startBinding";
roles[EndBindingRole] = "endBinding";
roles[BoundElementIdsRole] = "boundElementIds";
```

In `updateShape()`, add role mappings:

```cpp
else if (it.key() == QStringLiteral("startBinding")) changedRoles << StartBindingRole;
else if (it.key() == QStringLiteral("endBinding")) changedRoles << EndBindingRole;
else if (it.key() == QStringLiteral("boundElementIds")) changedRoles << BoundElementIdsRole;
```

- [ ] **Step 3: Write tests for binding data storage**

```cpp
void ShapesModelTest::testBindingDataModel()
{
    ShapesModel model;

    // Create a rectangle target
    QVariantMap rect;
    rect["type"] = "rectangle";
    rect["id"] = "rect-1";
    rect["x"] = 100.0;
    rect["y"] = 100.0;
    rect["width"] = 200.0;
    rect["height"] = 100.0;
    model.addShape(rect);

    // Create an arrow with startBinding
    QVariantMap arrow;
    arrow["type"] = "arrow";
    arrow["id"] = "arrow-1";
    arrow["fromX"] = 50.0;
    arrow["fromY"] = 50.0;
    arrow["toX"] = 100.0;
    arrow["toY"] = 150.0;
    QVariantMap binding;
    binding["elementId"] = "rect-1";
    binding["focus"] = 0.25;
    binding["gap"] = 0.0;
    arrow["startBinding"] = binding;
    model.addShape(arrow);

    // Verify binding is stored and retrievable
    QModelIndex idx = model.index(1);
    QVariant sb = model.data(idx, ShapesModel::StartBindingRole);
    QVERIFY(sb.isValid());
    QVariantMap sbMap = sb.toMap();
    QCOMPARE(sbMap["elementId"].toString(), QStringLiteral("rect-1"));
    QCOMPARE(sbMap["focus"].toDouble(), 0.25);

    // Verify endBinding is empty/invalid
    QVariant eb = model.data(idx, ShapesModel::EndBindingRole);
    QVERIFY(!eb.isValid() || eb.toMap().isEmpty());

    // Update endBinding
    QVariantMap endBinding;
    endBinding["elementId"] = "rect-1";
    endBinding["focus"] = 0.75;
    endBinding["gap"] = 0.0;
    model.updateShape(1, {{"endBinding", endBinding}});

    eb = model.data(idx, ShapesModel::EndBindingRole);
    QVERIFY(eb.isValid());
    QCOMPARE(eb.toMap()["focus"].toDouble(), 0.75);

    // Update back-references on the rect
    QVariantList boundIds;
    boundIds.append("arrow-1");
    model.updateShape(0, {{"boundElementIds", boundIds}});

    QModelIndex rectIdx = model.index(0);
    QVariant be = model.data(rectIdx, ShapesModel::BoundElementIdsRole);
    QVERIFY(be.isValid());
    QCOMPARE(be.toList().size(), 1);
    QCOMPARE(be.toList().at(0).toString(), QStringLiteral("arrow-1"));
}
```

Add `testBindingDataModel` to the private Q_SLOTS list.

- [ ] **Step 4: Build and run tests**

Run: `cd ~/coding/projects/scribbleway/build-deb && cmake .. && make -j$(nproc) && ctest --output-on-failure`

Expected: All tests pass including `testBindingDataModel`.

- [ ] **Step 5: Commit**

```bash
git add src/overlay/shapesmodel.h src/overlay/shapesmodel.cpp tests/shapesmodeltest.cpp
git commit -m "feat: add binding and back-reference roles to ShapesModel"
```

---

### Task 3: Binding Geometry Utilities in C++

Add the core geometry functions to OverlayController that compute snap points and recompute endpoints from bindings. These are pure computation — no model mutation.

**Files:**
- Modify: `src/overlay/overlaycontroller.h` (add private helpers)
- Modify: `src/overlay/overlaycontroller.cpp` (implement helpers)
- Test: `tests/shapesmodeltest.cpp`

**Interfaces:**
- Consumes: Shape data from `ShapesModel`
- Produces:
  - `struct BindingHit { QString targetId; double focus; QPointF snapPoint; };`
  - `BindingHit findSnapTarget(double px, double py, int excludeIndex) const;` — scans all rect/ellipse shapes, returns nearest edge point within 20px, or empty hit
  - `QPointF pointFromBinding(const QVariantMap &targetShape, double focus) const;` — given a target shape and focus, returns the absolute point on its edge
  - `double focusForPoint(const QVariantMap &targetShape, double px, double py) const;` — given a target shape and a point, returns the parametric focus

The `focus` parameterization:
- **Rectangle**: perimeter fraction 0.0–1.0 starting from center of top edge, going clockwise. So 0.0 = top-center, 0.25 = right-center, 0.5 = bottom-center, 0.75 = left-center.
- **Ellipse**: angle in radians [0, 2π) where 0 = top-center (−π/2 in standard math coords), going clockwise.

- [ ] **Step 1: Add BindingHit struct and helper declarations**

In `overlaycontroller.h`, before the class, add:

```cpp
struct BindingHit {
    QString targetId;
    double focus = 0.0;
    QPointF snapPoint;
    bool valid = false;
};
```

In the `private:` section of OverlayController, add:

```cpp
static constexpr double kSnapThreshold = 20.0;
BindingHit findSnapTarget(double px, double py, int excludeIndex = -1) const;
QPointF pointFromBinding(const QVariantMap &targetShape, double focus) const;
double focusForPoint(const QVariantMap &targetShape, double px, double py) const;
QPointF nearestPointOnRect(double rx, double ry, double rw, double rh, double px, double py) const;
QPointF nearestPointOnEllipse(double cx, double cy, double rx, double ry, double px, double py) const;
```

Add `#include <QtMath>` if not already present in overlaycontroller.cpp.

- [ ] **Step 2: Implement nearestPointOnRect**

```cpp
QPointF OverlayController::nearestPointOnRect(double rx, double ry, double rw, double rh, double px, double py) const
{
    // Clamp point to nearest position on rectangle perimeter
    // Check all four edges, return the closest point
    struct EdgePoint { QPointF p; double dist; };
    auto clamp = [](double v, double lo, double hi) { return qBound(lo, v, hi); };

    // Top edge: y = ry, x in [rx, rx+rw]
    QPointF top(clamp(px, rx, rx + rw), ry);
    // Bottom edge: y = ry+rh, x in [rx, rx+rw]
    QPointF bottom(clamp(px, rx, rx + rw), ry + rh);
    // Left edge: x = rx, y in [ry, ry+rh]
    QPointF left(rx, clamp(py, ry, ry + rh));
    // Right edge: x = rx+rw, y in [ry, ry+rh]
    QPointF right(rx + rw, clamp(py, ry, ry + rh));

    auto dist2 = [&](const QPointF &a) {
        double dx = a.x() - px, dy = a.y() - py;
        return dx * dx + dy * dy;
    };

    EdgePoint candidates[] = {
        {top, dist2(top)}, {bottom, dist2(bottom)},
        {left, dist2(left)}, {right, dist2(right)}
    };

    EdgePoint *best = &candidates[0];
    for (int i = 1; i < 4; ++i) {
        if (candidates[i].dist < best->dist) best = &candidates[i];
    }
    return best->p;
}
```

- [ ] **Step 3: Implement nearestPointOnEllipse**

```cpp
QPointF OverlayController::nearestPointOnEllipse(double cx, double cy, double a, double b, double px, double py) const
{
    // Iterative projection onto ellipse (Newton's method, 4 iterations is accurate enough)
    // Works in local coords centered on ellipse
    double lx = px - cx;
    double ly = py - cy;

    if (qFuzzyIsNull(lx) && qFuzzyIsNull(ly)) {
        // Point is at center — pick top
        return QPointF(cx, cy - b);
    }

    double theta = qAtan2(ly / b, lx / a);
    for (int i = 0; i < 4; ++i) {
        double ct = qCos(theta), st = qSin(theta);
        double ex = a * ct, ey = b * st;
        double dx = lx - ex, dy = ly - ey;
        double dtx = -a * st, dty = b * ct;
        double denom = dtx * dtx + dty * dty;
        if (qFuzzyIsNull(denom)) break;
        theta += (dx * dtx + dy * dty) / denom;
    }

    return QPointF(cx + a * qCos(theta), cy + b * qSin(theta));
}
```

- [ ] **Step 4: Implement focusForPoint**

```cpp
double OverlayController::focusForPoint(const QVariantMap &shape, double px, double py) const
{
    QString type = shape.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("rectangle")) {
        double rx = shape[QStringLiteral("x")].toDouble();
        double ry = shape[QStringLiteral("y")].toDouble();
        double rw = shape[QStringLiteral("width")].toDouble();
        double rh = shape[QStringLiteral("height")].toDouble();
        QPointF sp = nearestPointOnRect(rx, ry, rw, rh, px, py);

        // Perimeter fraction, clockwise from top-center
        double perim = 2.0 * (rw + rh);
        if (perim < 1e-6) return 0.0;

        double topCenter = rx + rw / 2.0;
        double d = 0.0;

        if (qFuzzyCompare(sp.y(), ry)) {
            // Top edge: distance from top-center going right
            d = sp.x() - topCenter;
            if (d < 0) d += perim; // wrap left side
        } else if (qFuzzyCompare(sp.x(), rx + rw)) {
            // Right edge
            d = rw / 2.0 + (sp.y() - ry);
        } else if (qFuzzyCompare(sp.y(), ry + rh)) {
            // Bottom edge (right to left)
            d = rw / 2.0 + rh + (rx + rw - sp.x());
        } else {
            // Left edge (bottom to top)
            d = rw / 2.0 + rh + rw + (ry + rh - sp.y());
        }

        return d / perim;
    } else if (type == QStringLiteral("ellipse")) {
        double cx = shape[QStringLiteral("x")].toDouble() + shape[QStringLiteral("width")].toDouble() / 2.0;
        double cy = shape[QStringLiteral("y")].toDouble() + shape[QStringLiteral("height")].toDouble() / 2.0;

        double angle = qAtan2(py - cy, px - cx);
        // Normalize to [0, 2π)
        if (angle < 0) angle += 2.0 * M_PI;
        return angle / (2.0 * M_PI);
    }

    return 0.0;
}
```

- [ ] **Step 5: Implement pointFromBinding**

```cpp
QPointF OverlayController::pointFromBinding(const QVariantMap &shape, double focus) const
{
    QString type = shape.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("rectangle")) {
        double rx = shape[QStringLiteral("x")].toDouble();
        double ry = shape[QStringLiteral("y")].toDouble();
        double rw = shape[QStringLiteral("width")].toDouble();
        double rh = shape[QStringLiteral("height")].toDouble();
        double perim = 2.0 * (rw + rh);
        double d = focus * perim;
        double topCenter = rx + rw / 2.0;

        // Walk clockwise from top-center
        // Segment 1: top-center to top-right = rw/2
        if (d <= rw / 2.0) {
            return QPointF(topCenter + d, ry);
        }
        d -= rw / 2.0;
        // Segment 2: right edge = rh
        if (d <= rh) {
            return QPointF(rx + rw, ry + d);
        }
        d -= rh;
        // Segment 3: bottom edge (right to left) = rw
        if (d <= rw) {
            return QPointF(rx + rw - d, ry + rh);
        }
        d -= rw;
        // Segment 4: left edge (bottom to top) = rh
        if (d <= rh) {
            return QPointF(rx, ry + rh - d);
        }
        d -= rh;
        // Segment 5: top edge (left to top-center) = rw/2
        return QPointF(rx + d, ry);
    } else if (type == QStringLiteral("ellipse")) {
        double cx = shape[QStringLiteral("x")].toDouble() + shape[QStringLiteral("width")].toDouble() / 2.0;
        double cy = shape[QStringLiteral("y")].toDouble() + shape[QStringLiteral("height")].toDouble() / 2.0;
        double a = shape[QStringLiteral("width")].toDouble() / 2.0;
        double b = shape[QStringLiteral("height")].toDouble() / 2.0;

        double angle = focus * 2.0 * M_PI;
        return QPointF(cx + a * qCos(angle), cy + b * qSin(angle));
    }

    return QPointF();
}
```

- [ ] **Step 6: Implement findSnapTarget**

```cpp
BindingHit OverlayController::findSnapTarget(double px, double py, int excludeIndex) const
{
    BindingHit best;
    double bestDist = kSnapThreshold + 1.0;

    const auto &shapes = m_shapesModel.shapes();
    for (int i = 0; i < shapes.size(); ++i) {
        if (i == excludeIndex) continue;

        const auto &shape = shapes[i];
        QString type = shape.value(QStringLiteral("type")).toString();
        if (type != QStringLiteral("rectangle") && type != QStringLiteral("ellipse"))
            continue;
        if (shape.value(QStringLiteral("locked"), false).toBool())
            continue;

        QPointF snapPt;
        if (type == QStringLiteral("rectangle")) {
            double rx = shape[QStringLiteral("x")].toDouble();
            double ry = shape[QStringLiteral("y")].toDouble();
            double rw = shape[QStringLiteral("width")].toDouble();
            double rh = shape[QStringLiteral("height")].toDouble();
            snapPt = nearestPointOnRect(rx, ry, rw, rh, px, py);
        } else {
            double cx = shape[QStringLiteral("x")].toDouble() + shape[QStringLiteral("width")].toDouble() / 2.0;
            double cy = shape[QStringLiteral("y")].toDouble() + shape[QStringLiteral("height")].toDouble() / 2.0;
            double a = shape[QStringLiteral("width")].toDouble() / 2.0;
            double b = shape[QStringLiteral("height")].toDouble() / 2.0;
            snapPt = nearestPointOnEllipse(cx, cy, a, b, px, py);
        }

        double dx = snapPt.x() - px, dy = snapPt.y() - py;
        double dist = qSqrt(dx * dx + dy * dy);
        if (dist <= kSnapThreshold && dist < bestDist) {
            bestDist = dist;
            best.targetId = shape.value(QStringLiteral("id")).toString();
            best.focus = focusForPoint(shape, snapPt.x(), snapPt.y());
            best.snapPoint = snapPt;
            best.valid = true;
        }
    }

    return best;
}
```

- [ ] **Step 7: Write geometry tests**

```cpp
void ShapesModelTest::testBindingGeometry()
{
    OverlayController ctrl;

    // Create a rectangle at (100, 100) size 200x100
    QVariantMap rect;
    rect["type"] = "rectangle";
    rect["id"] = "rect-geo";
    rect["x"] = 100.0;
    rect["y"] = 100.0;
    rect["width"] = 200.0;
    rect["height"] = 100.0;
    rect["selected"] = false;
    rect["locked"] = false;
    ctrl.addShape(rect);

    // pointFromBinding at focus=0.0 should be top-center (200, 100)
    QPointF p0 = ctrl.pointFromBinding(rect, 0.0);
    QCOMPARE(p0, QPointF(200.0, 100.0));

    // focus=0.25 should be right-center (300, 150)
    // rw/2=100 is first segment. perim = 600. 0.25 * 600 = 150. 150-100=50 into right edge -> (300, 150)
    QPointF p25 = ctrl.pointFromBinding(rect, 0.25);
    QCOMPARE(p25, QPointF(300.0, 150.0));

    // focus=0.5 should be bottom-center (200, 200)
    QPointF p50 = ctrl.pointFromBinding(rect, 0.5);
    QCOMPARE(p50, QPointF(200.0, 200.0));

    // findSnapTarget: point at (201, 95) is 5px from top edge -> should snap to (201, 100)
    auto hit = ctrl.findSnapTarget(201.0, 95.0);
    QVERIFY(hit.valid);
    QCOMPARE(hit.targetId, QStringLiteral("rect-geo"));
    QCOMPARE(hit.snapPoint.y(), 100.0);

    // Point at (201, 50) is 50px from top edge -> should NOT snap
    auto miss = ctrl.findSnapTarget(201.0, 50.0);
    QVERIFY(!miss.valid);

    // Create an ellipse
    QVariantMap ell;
    ell["type"] = "ellipse";
    ell["id"] = "ell-geo";
    ell["x"] = 400.0;
    ell["y"] = 100.0;
    ell["width"] = 100.0;
    ell["height"] = 60.0;
    ell["selected"] = false;
    ell["locked"] = false;
    ctrl.addShape(ell);

    // Point near right side of ellipse -> should snap
    auto hitEll = ctrl.findSnapTarget(505.0, 130.0);
    QVERIFY(hitEll.valid);
    QCOMPARE(hitEll.targetId, QStringLiteral("ell-geo"));
    // snapPoint should be on the ellipse perimeter, close to (500, 130)
    QVERIFY(qAbs(hitEll.snapPoint.x() - 500.0) < 5.0);
}
```

Add `testBindingGeometry` to private Q_SLOTS. Also make `pointFromBinding` and `findSnapTarget` public or friend the test class. Simplest: add as `Q_INVOKABLE` public methods (they're useful from QML too):

```cpp
// In overlaycontroller.h, public section:
Q_INVOKABLE QPointF pointFromBinding(const QVariantMap &targetShape, double focus) const;
Q_INVOKABLE QPointF findSnapPoint(double px, double py, int excludeIndex = -1) const;
```

Where `findSnapPoint` is a thin QML-friendly wrapper returning just the point (or QPointF() if no hit). The full `findSnapTarget` stays private.

- [ ] **Step 8: Build and run tests**

Run: `cd ~/coding/projects/scribbleway/build-deb && cmake .. && make -j$(nproc) && ctest --output-on-failure`

Expected: All tests pass including `testBindingGeometry`.

- [ ] **Step 9: Commit**

```bash
git add src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp tests/shapesmodeltest.cpp
git commit -m "feat: add binding geometry utilities (snap, focus, point reconstruction)"
```

---

### Task 4: Binding Creation on Draw Finalization and Move Propagation

When a line/arrow is finalized, check each endpoint for snap targets and create bindings. When a bound rect/ellipse is moved (drag or nudge), update all connected endpoints. When a bound rect/ellipse is resized, update endpoints. When a shape with bindings is deleted, clean up back-references.

This is the core behavior task. It modifies `OverlayController` methods and `main.qml`'s `finalizeShape()`.

**Files:**
- Modify: `src/overlay/overlaycontroller.h` (add helper: `updateBoundEndpoints`, `cleanupBindingsForDelete`, `createBindingsForShape`)
- Modify: `src/overlay/overlaycontroller.cpp` (modify `addShape`, `dragSelected`, `nudgeSelected`, `deleteShape`; add new methods)
- Modify: `src/overlay/qml/main.qml:138-206` (finalizeShape — snap endpoints)
- Test: `tests/shapesmodeltest.cpp`

**Interfaces:**
- Consumes: `findSnapTarget`, `pointFromBinding`, `focusForPoint`, `indexForId`, `shapeIdAt` from Tasks 1–3
- Produces:
  - `void createBindingsForShape(int lineIndex)` — scans both endpoints of a line/arrow at `lineIndex`, finds snap targets, stores bindings on the line and back-references on targets
  - `void updateBoundEndpoints(int movedShapeIndex)` — given a rect/ellipse that just moved/resized, finds all lines/arrows bound to it and updates their endpoints
  - `void cleanupBindingsForDelete(int deletedIndex)` — removes all binding references to/from the shape being deleted
  - `void breakBinding(int lineIndex, bool isStart)` — removes a specific binding from a line/arrow and its back-reference

- [ ] **Step 1: Add method declarations to OverlayController**

In `overlaycontroller.h`, private section:

```cpp
void createBindingsForShape(int lineIndex);
void updateBoundEndpoints(int shapeIndex);
void cleanupBindingsForDelete(int deletedIndex);
void breakBinding(int lineIndex, bool isStart);
void addBackReference(const QString &targetId, const QString &lineId);
void removeBackReference(const QString &targetId, const QString &lineId);
```

Make `createBindingsForShape` public Q_INVOKABLE so QML can call it after finalize:

```cpp
Q_INVOKABLE void createBindingsForShape(int lineIndex);
```

- [ ] **Step 2: Implement addBackReference and removeBackReference**

```cpp
void OverlayController::addBackReference(const QString &targetId, const QString &lineId)
{
    int targetIdx = indexForId(targetId);
    if (targetIdx < 0) return;
    QVariantMap shape = m_shapesModel.shapes()[targetIdx];
    QVariantList boundIds = shape.value(QStringLiteral("boundElementIds")).toList();
    if (!boundIds.contains(lineId)) {
        boundIds.append(lineId);
        m_shapesModel.updateShape(targetIdx, {{QStringLiteral("boundElementIds"), boundIds}});
    }
}

void OverlayController::removeBackReference(const QString &targetId, const QString &lineId)
{
    int targetIdx = indexForId(targetId);
    if (targetIdx < 0) return;
    QVariantMap shape = m_shapesModel.shapes()[targetIdx];
    QVariantList boundIds = shape.value(QStringLiteral("boundElementIds")).toList();
    boundIds.removeAll(lineId);
    m_shapesModel.updateShape(targetIdx, {{QStringLiteral("boundElementIds"), boundIds}});
}
```

- [ ] **Step 3: Implement createBindingsForShape**

```cpp
void OverlayController::createBindingsForShape(int lineIndex)
{
    if (lineIndex < 0 || lineIndex >= m_shapesModel.rowCount()) return;
    const auto &shapes = m_shapesModel.shapes();
    const auto &lineShape = shapes[lineIndex];
    QString type = lineShape.value(QStringLiteral("type")).toString();
    if (type != QStringLiteral("line") && type != QStringLiteral("arrow")) return;

    QString lineId = lineShape.value(QStringLiteral("id")).toString();
    double fromX = lineShape[QStringLiteral("fromX")].toDouble();
    double fromY = lineShape[QStringLiteral("fromY")].toDouble();
    double toX = lineShape[QStringLiteral("toX")].toDouble();
    double toY = lineShape[QStringLiteral("toY")].toDouble();

    QVariantMap updates;

    // Check start endpoint
    BindingHit startHit = findSnapTarget(fromX, fromY, lineIndex);
    if (startHit.valid) {
        QVariantMap sb;
        sb[QStringLiteral("elementId")] = startHit.targetId;
        sb[QStringLiteral("focus")] = startHit.focus;
        sb[QStringLiteral("gap")] = 0.0;
        updates[QStringLiteral("startBinding")] = sb;
        updates[QStringLiteral("fromX")] = startHit.snapPoint.x();
        updates[QStringLiteral("fromY")] = startHit.snapPoint.y();
        addBackReference(startHit.targetId, lineId);
    }

    // Check end endpoint
    BindingHit endHit = findSnapTarget(toX, toY, lineIndex);
    if (endHit.valid) {
        QVariantMap eb;
        eb[QStringLiteral("elementId")] = endHit.targetId;
        eb[QStringLiteral("focus")] = endHit.focus;
        eb[QStringLiteral("gap")] = 0.0;
        updates[QStringLiteral("endBinding")] = eb;
        updates[QStringLiteral("toX")] = endHit.snapPoint.x();
        updates[QStringLiteral("toY")] = endHit.snapPoint.y();
        addBackReference(endHit.targetId, lineId);
    }

    if (!updates.isEmpty()) {
        m_shapesModel.updateShape(lineIndex, updates);
    }
}
```

- [ ] **Step 4: Implement updateBoundEndpoints**

```cpp
void OverlayController::updateBoundEndpoints(int shapeIndex)
{
    if (shapeIndex < 0 || shapeIndex >= m_shapesModel.rowCount()) return;
    const auto &targetShape = m_shapesModel.shapes()[shapeIndex];
    QString targetId = targetShape.value(QStringLiteral("id")).toString();
    QVariantList boundIds = targetShape.value(QStringLiteral("boundElementIds")).toList();

    for (const QVariant &bidVar : boundIds) {
        QString lineId = bidVar.toString();
        int lineIdx = indexForId(lineId);
        if (lineIdx < 0) continue;

        const auto &lineShape = m_shapesModel.shapes()[lineIdx];
        QVariantMap updates;

        QVariantMap sb = lineShape.value(QStringLiteral("startBinding")).toMap();
        if (sb.value(QStringLiteral("elementId")).toString() == targetId) {
            QPointF p = pointFromBinding(targetShape, sb.value(QStringLiteral("focus")).toDouble());
            updates[QStringLiteral("fromX")] = p.x();
            updates[QStringLiteral("fromY")] = p.y();
        }

        QVariantMap eb = lineShape.value(QStringLiteral("endBinding")).toMap();
        if (eb.value(QStringLiteral("elementId")).toString() == targetId) {
            QPointF p = pointFromBinding(targetShape, eb.value(QStringLiteral("focus")).toDouble());
            updates[QStringLiteral("toX")] = p.x();
            updates[QStringLiteral("toY")] = p.y();
        }

        if (!updates.isEmpty()) {
            m_shapesModel.updateShape(lineIdx, updates);
        }
    }
}
```

- [ ] **Step 5: Implement breakBinding**

```cpp
void OverlayController::breakBinding(int lineIndex, bool isStart)
{
    if (lineIndex < 0 || lineIndex >= m_shapesModel.rowCount()) return;
    const auto &lineShape = m_shapesModel.shapes()[lineIndex];
    QString lineId = lineShape.value(QStringLiteral("id")).toString();
    QString bindingKey = isStart ? QStringLiteral("startBinding") : QStringLiteral("endBinding");

    QVariantMap binding = lineShape.value(bindingKey).toMap();
    if (binding.isEmpty()) return;

    QString targetId = binding.value(QStringLiteral("elementId")).toString();
    removeBackReference(targetId, lineId);

    // Clear the binding by setting it to an empty map
    m_shapesModel.updateShape(lineIndex, {{bindingKey, QVariantMap()}});
}
```

- [ ] **Step 6: Implement cleanupBindingsForDelete**

```cpp
void OverlayController::cleanupBindingsForDelete(int deletedIndex)
{
    if (deletedIndex < 0 || deletedIndex >= m_shapesModel.rowCount()) return;
    const auto &shape = m_shapesModel.shapes()[deletedIndex];
    QString deletedId = shape.value(QStringLiteral("id")).toString();
    QString type = shape.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        // Remove back-references from any bound targets
        QVariantMap sb = shape.value(QStringLiteral("startBinding")).toMap();
        if (!sb.isEmpty()) {
            removeBackReference(sb.value(QStringLiteral("elementId")).toString(), deletedId);
        }
        QVariantMap eb = shape.value(QStringLiteral("endBinding")).toMap();
        if (!eb.isEmpty()) {
            removeBackReference(eb.value(QStringLiteral("elementId")).toString(), deletedId);
        }
    } else if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
        // Remove bindings from all lines/arrows bound to this shape
        QVariantList boundIds = shape.value(QStringLiteral("boundElementIds")).toList();
        for (const QVariant &bidVar : boundIds) {
            int lineIdx = indexForId(bidVar.toString());
            if (lineIdx < 0) continue;
            const auto &lineShape = m_shapesModel.shapes()[lineIdx];
            QVariantMap updates;

            QVariantMap sb = lineShape.value(QStringLiteral("startBinding")).toMap();
            if (sb.value(QStringLiteral("elementId")).toString() == deletedId) {
                updates[QStringLiteral("startBinding")] = QVariantMap();
            }
            QVariantMap eb = lineShape.value(QStringLiteral("endBinding")).toMap();
            if (eb.value(QStringLiteral("elementId")).toString() == deletedId) {
                updates[QStringLiteral("endBinding")] = QVariantMap();
            }
            if (!updates.isEmpty()) {
                m_shapesModel.updateShape(lineIdx, updates);
            }
        }
    }
}
```

- [ ] **Step 7: Hook into deleteShape**

Modify `OverlayController::deleteShape()` to call cleanup before removing:

```cpp
void OverlayController::deleteShape(int index)
{
    if (index >= 0 && index < m_shapesModel.rowCount()) {
        cleanupBindingsForDelete(index);
        m_shapesModel.removeShape(index);
        if (m_selectedIndex == index) {
            m_selectedIndex = -1;
            notifySelectionChanged();
        } else if (m_selectedIndex > index) {
            m_selectedIndex--;
            notifySelectionChanged();
        }
        notifyShapesChanged();
    }
}
```

- [ ] **Step 8: Hook into dragSelected**

In `dragSelected()`, after the existing for-loop that moves shapes (before `m_shapesModel.endEdit()`), add endpoint update pass:

```cpp
    // After moving all selected shapes, update bound endpoints
    for (auto it = m_dragStartShapes.begin(); it != m_dragStartShapes.end(); ++it) {
        int index = it.key();
        QString type = it.value()[QStringLiteral("type")].toString();
        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
            updateBoundEndpoints(index);
        }
    }
```

But skip updating endpoints of lines that are ALSO being dragged (both shape and connected line selected). Add a check in `updateBoundEndpoints`: skip lines that are in `m_dragStartShapes`:

Modify `updateBoundEndpoints` to accept an optional skip set:

```cpp
void OverlayController::updateBoundEndpoints(int shapeIndex, const QSet<int> *skipIndices)
```

And in the loop:
```cpp
if (skipIndices && skipIndices->contains(lineIdx)) continue;
```

Build the skip set from `m_dragStartShapes.keys()` in `dragSelected`:

```cpp
    QSet<int> draggedIndices;
    for (auto it = m_dragStartShapes.begin(); it != m_dragStartShapes.end(); ++it) {
        draggedIndices.insert(it.key());
    }
    for (auto it = m_dragStartShapes.begin(); it != m_dragStartShapes.end(); ++it) {
        QString type = it.value()[QStringLiteral("type")].toString();
        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
            updateBoundEndpoints(it.key(), &draggedIndices);
        }
    }
```

- [ ] **Step 9: Hook into nudgeSelected**

Same pattern as dragSelected. After the existing loop that moves shapes (before `m_shapesModel.endEdit()`):

```cpp
    QSet<int> nudgedIndices;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        QVariantMap shape = m_shapesModel.shapes()[i];
        if (shape.value(QStringLiteral("selected"), false).toBool() &&
            !shape.value(QStringLiteral("locked"), false).toBool()) {
            nudgedIndices.insert(i);
        }
    }
    for (int idx : nudgedIndices) {
        QString type = m_shapesModel.shapes()[idx][QStringLiteral("type")].toString();
        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
            updateBoundEndpoints(idx, &nudgedIndices);
        }
    }
```

- [ ] **Step 10: Hook finalizeShape in main.qml**

In `main.qml` `finalizeShape()`, after `controller.addShape(shape)` for line/arrow (around line 175), add:

```javascript
controller.createBindingsForShape(controller.shapesModel.rowCount() - 1);
```

The full block becomes:

```javascript
} else if (activeDrawTool === "line" || activeDrawTool === "arrow") {
    let dx = previewW;
    let dy = previewH;
    if (Math.abs(dx) > 2 || Math.abs(dy) > 2) {
        shape["fromX"] = drawStartPoint.x;
        shape["fromY"] = drawStartPoint.y;
        shape["toX"] = drawStartPoint.x + dx;
        shape["toY"] = drawStartPoint.y + dy;
        controller.addShape(shape);
        controller.createBindingsForShape(controller.shapesModel.rowCount() - 1);
    }
```

- [ ] **Step 11: Write integration tests**

```cpp
void ShapesModelTest::testBindingMovePropagate()
{
    OverlayController ctrl;

    // Create a rectangle
    QVariantMap rect;
    rect["type"] = "rectangle";
    rect["id"] = "rect-bind";
    rect["x"] = 100.0;
    rect["y"] = 100.0;
    rect["width"] = 200.0;
    rect["height"] = 100.0;
    rect["selected"] = false;
    rect["locked"] = false;
    ctrl.addShape(rect);

    // Create an arrow with endpoint on the rect's top edge
    QVariantMap arrow;
    arrow["type"] = "arrow";
    arrow["id"] = "arrow-bind";
    arrow["fromX"] = 50.0;
    arrow["fromY"] = 50.0;
    arrow["toX"] = 200.0;  // top-center of rect
    arrow["toY"] = 100.0;
    arrow["selected"] = false;
    arrow["locked"] = false;
    ctrl.addShape(arrow);

    // Create bindings
    ctrl.createBindingsForShape(1);

    // Verify endBinding was created
    QVariantMap arrowData = ctrl.getShape(1);
    QVariantMap eb = arrowData["endBinding"].toMap();
    QVERIFY(!eb.isEmpty());
    QCOMPARE(eb["elementId"].toString(), QStringLiteral("rect-bind"));
    // toX/toY should be snapped to (200, 100)
    QCOMPARE(arrowData["toX"].toDouble(), 200.0);
    QCOMPARE(arrowData["toY"].toDouble(), 100.0);

    // Verify back-reference on rect
    QVariantMap rectData = ctrl.getShape(0);
    QVariantList boundIds = rectData["boundElementIds"].toList();
    QCOMPARE(boundIds.size(), 1);
    QCOMPARE(boundIds[0].toString(), QStringLiteral("arrow-bind"));

    // Select the rect and drag it 50px right
    ctrl.selectShape(0);
    ctrl.beginEdit();
    ctrl.dragSelected(50.0, 0.0);
    ctrl.endEdit();

    // Arrow endpoint should have moved with the rect
    arrowData = ctrl.getShape(1);
    QCOMPARE(arrowData["toX"].toDouble(), 250.0); // 200 + 50
    QCOMPARE(arrowData["toY"].toDouble(), 100.0);

    // Arrow fromX/fromY should NOT have moved (not bound)
    QCOMPARE(arrowData["fromX"].toDouble(), 50.0);
    QCOMPARE(arrowData["fromY"].toDouble(), 50.0);
}

void ShapesModelTest::testBindingDeleteCleanup()
{
    OverlayController ctrl;

    // Create rect and bound arrow
    QVariantMap rect;
    rect["type"] = "rectangle";
    rect["id"] = "rect-del";
    rect["x"] = 100.0; rect["y"] = 100.0;
    rect["width"] = 200.0; rect["height"] = 100.0;
    rect["selected"] = false; rect["locked"] = false;
    ctrl.addShape(rect);

    QVariantMap arrow;
    arrow["type"] = "arrow";
    arrow["id"] = "arrow-del";
    arrow["fromX"] = 50.0; arrow["fromY"] = 50.0;
    arrow["toX"] = 200.0; arrow["toY"] = 100.0;
    arrow["selected"] = false; arrow["locked"] = false;
    ctrl.addShape(arrow);
    ctrl.createBindingsForShape(1);

    // Delete the rect -> arrow's endBinding should be cleared
    ctrl.deleteShape(0);

    // Arrow is now at index 0
    QVariantMap arrowData = ctrl.getShape(0);
    QVariantMap eb = arrowData["endBinding"].toMap();
    QVERIFY(eb.isEmpty());
    // Arrow endpoint stays where it was (not moved)
    QCOMPARE(arrowData["toX"].toDouble(), 200.0);
}
```

Add `testBindingMovePropagate` and `testBindingDeleteCleanup` to private Q_SLOTS.

- [ ] **Step 12: Build and run tests**

Run: `cd ~/coding/projects/scribbleway/build-deb && cmake .. && make -j$(nproc) && ctest --output-on-failure`

Expected: All tests pass.

- [ ] **Step 13: Commit**

```bash
git add src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp src/overlay/qml/main.qml tests/shapesmodeltest.cpp
git commit -m "feat: binding creation, move propagation, and delete cleanup"
```

---

### Task 5: Endpoint Drag — Break and Re-snap Bindings

When the user drags an endpoint handle on a bound line/arrow, the binding must break immediately. On release, if the endpoint is near a snap target, a new binding is created.

**Files:**
- Modify: `src/overlay/qml/shapes/BaseShape.qml:312-351` (endpoint handle drag)
- Modify: `src/overlay/overlaycontroller.h` (add Q_INVOKABLE breakBinding, resnap)
- Modify: `src/overlay/overlaycontroller.cpp`
- Test: `tests/shapesmodeltest.cpp`

**Interfaces:**
- Consumes: `breakBinding`, `createBindingsForShape`, `findSnapTarget` from Tasks 3–4
- Produces:
  - `Q_INVOKABLE void breakEndpointBinding(int lineIndex, bool isStart)` — public wrapper for QML
  - `Q_INVOKABLE void resnapEndpoint(int lineIndex, bool isStart)` — called on release; checks for snap and creates binding

- [ ] **Step 1: Add public Q_INVOKABLE wrappers**

In `overlaycontroller.h`, public section:

```cpp
Q_INVOKABLE void breakEndpointBinding(int lineIndex, bool isStart);
Q_INVOKABLE void resnapEndpoint(int lineIndex, bool isStart);
```

In `overlaycontroller.cpp`:

```cpp
void OverlayController::breakEndpointBinding(int lineIndex, bool isStart)
{
    breakBinding(lineIndex, isStart);
}

void OverlayController::resnapEndpoint(int lineIndex, bool isStart)
{
    if (lineIndex < 0 || lineIndex >= m_shapesModel.rowCount()) return;
    const auto &lineShape = m_shapesModel.shapes()[lineIndex];
    QString type = lineShape.value(QStringLiteral("type")).toString();
    if (type != QStringLiteral("line") && type != QStringLiteral("arrow")) return;

    double px = isStart ? lineShape[QStringLiteral("fromX")].toDouble()
                        : lineShape[QStringLiteral("toX")].toDouble();
    double py = isStart ? lineShape[QStringLiteral("fromY")].toDouble()
                        : lineShape[QStringLiteral("toY")].toDouble();

    BindingHit hit = findSnapTarget(px, py, lineIndex);
    if (!hit.valid) return;

    QString lineId = lineShape.value(QStringLiteral("id")).toString();
    QString bindingKey = isStart ? QStringLiteral("startBinding") : QStringLiteral("endBinding");
    QString xKey = isStart ? QStringLiteral("fromX") : QStringLiteral("toX");
    QString yKey = isStart ? QStringLiteral("fromY") : QStringLiteral("toY");

    QVariantMap binding;
    binding[QStringLiteral("elementId")] = hit.targetId;
    binding[QStringLiteral("focus")] = hit.focus;
    binding[QStringLiteral("gap")] = 0.0;

    m_shapesModel.updateShape(lineIndex, {
        {bindingKey, binding},
        {xKey, hit.snapPoint.x()},
        {yKey, hit.snapPoint.y()}
    });
    addBackReference(hit.targetId, lineId);
}
```

- [ ] **Step 2: Modify BaseShape endpoint drag handlers**

In `BaseShape.qml`, in the endpoint handle MouseArea (line ~312):

Modify `onPressed` to break the binding when drag starts:

```qml
onPressed: (mouse) => {
    controller.beginEdit();
    baseShapeRoot.isResizing = true;
    let pos = mapToItem(baseShapeRoot, mouse.x, mouse.y);
    dragStartX = pos.x;
    dragStartY = pos.y;
    originalX = index === 0 ? shapeFromX : shapeToX;
    originalY = index === 0 ? shapeFromY : shapeToY;
    // Break any existing binding on this endpoint
    controller.breakEndpointBinding(shapeIndex, index === 0);
}
```

Modify `onReleased` to attempt re-snap:

```qml
onReleased: {
    // Try to snap to a nearby shape
    controller.resnapEndpoint(shapeIndex, index === 0);
    baseShapeRoot.isResizing = false;
    controller.endEdit();
}
```

- [ ] **Step 3: Write tests**

```cpp
void ShapesModelTest::testBindingBreakAndResnap()
{
    OverlayController ctrl;

    // Create two rects
    QVariantMap rect1;
    rect1["type"] = "rectangle"; rect1["id"] = "r1";
    rect1["x"] = 100.0; rect1["y"] = 100.0;
    rect1["width"] = 100.0; rect1["height"] = 100.0;
    rect1["selected"] = false; rect1["locked"] = false;
    ctrl.addShape(rect1);

    QVariantMap rect2;
    rect2["type"] = "rectangle"; rect2["id"] = "r2";
    rect2["x"] = 400.0; rect2["y"] = 100.0;
    rect2["width"] = 100.0; rect2["height"] = 100.0;
    rect2["selected"] = false; rect2["locked"] = false;
    ctrl.addShape(rect2);

    // Arrow from rect1 right edge to rect2 left edge
    QVariantMap arrow;
    arrow["type"] = "arrow"; arrow["id"] = "a1";
    arrow["fromX"] = 200.0; arrow["fromY"] = 150.0;  // right edge of rect1
    arrow["toX"] = 400.0; arrow["toY"] = 150.0;      // left edge of rect2
    arrow["selected"] = false; arrow["locked"] = false;
    ctrl.addShape(arrow);
    ctrl.createBindingsForShape(2);

    // Verify both bindings exist
    QVariantMap ad = ctrl.getShape(2);
    QVERIFY(!ad["startBinding"].toMap().isEmpty());
    QVERIFY(!ad["endBinding"].toMap().isEmpty());

    // Break start binding
    ctrl.breakEndpointBinding(2, true);
    ad = ctrl.getShape(2);
    QVERIFY(ad["startBinding"].toMap().isEmpty());
    QVERIFY(!ad["endBinding"].toMap().isEmpty()); // end still bound

    // Re-snap start: move fromX to be near rect1 again
    ctrl.updateShape(2, {{"fromX", 200.0}, {"fromY", 150.0}});
    ctrl.resnapEndpoint(2, true);
    ad = ctrl.getShape(2);
    QVERIFY(!ad["startBinding"].toMap().isEmpty()); // re-bound
}
```

Add `testBindingBreakAndResnap` to private Q_SLOTS.

- [ ] **Step 4: Build and run tests**

Run: `cd ~/coding/projects/scribbleway/build-deb && cmake .. && make -j$(nproc) && ctest --output-on-failure`

- [ ] **Step 5: Commit**

```bash
git add src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp src/overlay/qml/shapes/BaseShape.qml tests/shapesmodeltest.cpp
git commit -m "feat: break binding on endpoint drag, re-snap on release"
```

---

### Task 6: Resize Propagation

When a bound rectangle/ellipse is resized via corner/midpoint handles, the attached endpoints must be recomputed from their `focus` parameter against the new geometry.

**Files:**
- Modify: `src/overlay/qml/shapes/RectangleShape.qml:14-16` (onRectGeometryChanged)
- Modify: `src/overlay/qml/shapes/EllipseShape.qml:14-16` (onRectGeometryChanged)
- Modify: `src/overlay/overlaycontroller.cpp` (updateShape — trigger endpoint update after geometry change of a bound shape)

**Interfaces:**
- Consumes: `updateBoundEndpoints` from Task 4, `BoundElementIdsRole`

- [ ] **Step 1: Add binding update after geometry changes in updateShape**

The simplest approach: in `OverlayController::updateShape()`, after the model update, check if the shape has `boundElementIds` and if geometry keys (x, y, width, height) changed. If so, call `updateBoundEndpoints`.

In `overlaycontroller.cpp`, modify `updateShape`:

```cpp
void OverlayController::updateShape(int index, const QVariantMap &properties)
{
    QVariantMap demarshalledProps = DBusUtils::demarshal(properties).toMap();
    m_shapesModel.updateShape(index, demarshalledProps);
    notifyShapesChanged();

    if (index == m_selectedIndex) {
        notifySelectionChanged();
    }

    // If geometry of a bindable shape changed, update bound endpoints
    bool geometryChanged = demarshalledProps.contains(QStringLiteral("x")) ||
                           demarshalledProps.contains(QStringLiteral("y")) ||
                           demarshalledProps.contains(QStringLiteral("width")) ||
                           demarshalledProps.contains(QStringLiteral("height"));
    if (geometryChanged) {
        const auto &shape = m_shapesModel.shapes()[index];
        if (!shape.value(QStringLiteral("boundElementIds")).toList().isEmpty()) {
            updateBoundEndpoints(index);
        }
    }
}
```

This automatically handles resize from corner handles (which call `rectGeometryChanged` → `controller.updateShape`) and from `growSelected`/`shrinkSelected` (which call `updateProperties` → `updateShape`).

- [ ] **Step 2: Write test**

```cpp
void ShapesModelTest::testBindingResizePropagate()
{
    OverlayController ctrl;

    // Rect at (100,100) size 200x100
    QVariantMap rect;
    rect["type"] = "rectangle"; rect["id"] = "rect-resize";
    rect["x"] = 100.0; rect["y"] = 100.0;
    rect["width"] = 200.0; rect["height"] = 100.0;
    rect["selected"] = false; rect["locked"] = false;
    ctrl.addShape(rect);

    // Arrow endpoint at top-center (200, 100)
    QVariantMap arrow;
    arrow["type"] = "arrow"; arrow["id"] = "arrow-resize";
    arrow["fromX"] = 50.0; arrow["fromY"] = 50.0;
    arrow["toX"] = 200.0; arrow["toY"] = 100.0;
    arrow["selected"] = false; arrow["locked"] = false;
    ctrl.addShape(arrow);
    ctrl.createBindingsForShape(1);

    // Resize rect: double width to 400
    ctrl.updateShape(0, {{"width", 400.0}});

    // Arrow endpoint should now be at top-center of wider rect: (100 + 400/2, 100) = (300, 100)
    QVariantMap ad = ctrl.getShape(1);
    QCOMPARE(ad["toX"].toDouble(), 300.0);
    QCOMPARE(ad["toY"].toDouble(), 100.0);
}
```

Add `testBindingResizePropagate` to private Q_SLOTS.

- [ ] **Step 3: Build and run tests**

Run: `cd ~/coding/projects/scribbleway/build-deb && cmake .. && make -j$(nproc) && ctest --output-on-failure`

- [ ] **Step 4: Commit**

```bash
git add src/overlay/overlaycontroller.cpp tests/shapesmodeltest.cpp
git commit -m "feat: update bound endpoints on shape resize"
```

---

### Task 7: Visual Snap Feedback During Drawing

Highlight the target shape and show a snap indicator dot while drawing a line/arrow with an endpoint within snap range. This requires:
1. A QML-accessible method to query the snap target and point during drawing
2. Visual elements in main.qml that show the highlight and snap dot

**Files:**
- Modify: `src/overlay/overlaycontroller.h` (add Q_INVOKABLE findSnapInfo for QML)
- Modify: `src/overlay/overlaycontroller.cpp`
- Modify: `src/overlay/qml/main.qml` (add snap indicator, modify preview layer, highlight shape during draw)
- Modify: `src/overlay/qml/shapes/BaseShape.qml` (add snapHighlight property)

**Interfaces:**
- Consumes: `findSnapTarget` from Task 3
- Produces:
  - `Q_INVOKABLE QVariantMap findSnapInfo(double px, double py, int excludeIndex = -1)` — returns `{ "valid": bool, "targetIndex": int, "snapX": double, "snapY": double }` for QML

- [ ] **Step 1: Add findSnapInfo Q_INVOKABLE**

In `overlaycontroller.h`, public section:

```cpp
Q_INVOKABLE QVariantMap findSnapInfo(double px, double py, int excludeIndex = -1) const;
```

In `overlaycontroller.cpp`:

```cpp
QVariantMap OverlayController::findSnapInfo(double px, double py, int excludeIndex) const
{
    QVariantMap result;
    BindingHit hit = findSnapTarget(px, py, excludeIndex);
    result[QStringLiteral("valid")] = hit.valid;
    if (hit.valid) {
        result[QStringLiteral("targetIndex")] = indexForId(hit.targetId);
        result[QStringLiteral("snapX")] = hit.snapPoint.x();
        result[QStringLiteral("snapY")] = hit.snapPoint.y();
    } else {
        result[QStringLiteral("targetIndex")] = -1;
        result[QStringLiteral("snapX")] = 0.0;
        result[QStringLiteral("snapY")] = 0.0;
    }
    return result;
}
```

- [ ] **Step 2: Add snap state properties to main.qml**

In `main.qml`, in the Window properties section (after `selectH`):

```qml
// Snap indicator state
property int snapTargetIndex: -1
property real snapPointX: 0
property real snapPointY: 0
property bool hasSnap: snapTargetIndex >= 0
```

- [ ] **Step 3: Update snap state during line/arrow drawing**

In `main.qml`, in `drawMouseArea.onPositionChanged`, after computing `previewW`/`previewH` for line/arrow (inside the `if (activeDrawTool === "line" || activeDrawTool === "arrow")` block), add snap detection:

```javascript
if (activeDrawTool === "line" || activeDrawTool === "arrow") {
    previewW = mouse.x - drawStartPoint.x;
    previewH = mouse.y - drawStartPoint.y;

    // Check snap for the endpoint being drawn (toX/toY)
    let endSnap = controller.findSnapInfo(mouse.x, mouse.y);
    // Also check snap for start point
    let startSnap = controller.findSnapInfo(drawStartPoint.x, drawStartPoint.y);

    // Show snap for the end point (more relevant during active draw)
    if (endSnap.valid) {
        snapTargetIndex = endSnap.targetIndex;
        snapPointX = endSnap.snapX;
        snapPointY = endSnap.snapY;
    } else if (startSnap.valid) {
        snapTargetIndex = startSnap.targetIndex;
        snapPointX = startSnap.snapX;
        snapPointY = startSnap.snapY;
    } else {
        snapTargetIndex = -1;
    }
}
```

Clear snap on release (in `onReleased`):

```javascript
onReleased: {
    snapTargetIndex = -1;
    finalizeShape();
}
```

Also clear on pressed (in `onPressed`):
```javascript
snapTargetIndex = -1;
```

- [ ] **Step 4: Add snap indicator dot and shape highlight**

In `main.qml`, after the arrow live preview (after line ~469), add the snap indicator:

```qml
// Snap indicator dot
Rectangle {
    visible: hasSnap && isDrawing && (activeDrawTool === "line" || activeDrawTool === "arrow")
    x: snapPointX - 5
    y: snapPointY - 5
    width: 10
    height: 10
    radius: 5
    color: "#3b82f6"
    border.color: "white"
    border.width: 1.5
    z: 999
}
```

- [ ] **Step 5: Add highlight border on snap target shape**

In `BaseShape.qml`, add a property for snap highlighting:

```qml
property bool isSnapTarget: typeof canvasWindow !== "undefined" && canvasWindow.snapTargetIndex === shapeIndex
```

Add a highlight rectangle (for rect mode) and highlight shape (for general use). After the selection border Item (at the end of BaseShape.qml, before the closing `}`):

```qml
// Snap target highlight
Rectangle {
    visible: isSnapTarget && mode === "rect"
    x: shapeX - 3
    y: shapeY - 3
    width: shapeWidth + 6
    height: shapeHeight + 6
    color: "transparent"
    border.color: "#3b82f6"
    border.width: 2
    radius: 4
    opacity: 0.8
}
```

- [ ] **Step 6: Build and smoke test**

Run: `cd ~/coding/projects/scribbleway/build-deb && cmake .. && make -j$(nproc)`

Manual smoke test: launch the overlay, draw a rectangle, then draw an arrow toward its edge. Verify:
- Blue dot appears on the rect edge when within 20px
- Rect gets a blue highlight border
- Both disappear when you move the cursor away

- [ ] **Step 7: Commit**

```bash
git add src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp src/overlay/qml/main.qml src/overlay/qml/shapes/BaseShape.qml
git commit -m "feat: visual snap feedback during line/arrow drawing"
```

---

### Task 8: Excalidraw Binding Compatibility

Map bindings to/from Excalidraw's clipboard format. Excalidraw stores `startBinding` and `endBinding` on arrow elements with `{ elementId, focus, gap }`, and `boundElements` on shapes with `[{ id, type }]`.

**Files:**
- Modify: `src/overlay/overlaycontroller.cpp:1132-1258` (convertToExcalidraw)
- Modify: `src/overlay/overlaycontroller.cpp:1260-1356` (convertFromExcalidraw)
- Modify: `src/overlay/overlaycontroller.cpp:972-992` (copySelected — post-process for bindings)
- Modify: `src/overlay/overlaycontroller.cpp:994-1130` (pasteFromClipboard — create bindings after paste)
- Test: `tests/shapesmodeltest.cpp`

**Interfaces:**
- Consumes: `createBindingsForShape`, binding data model from Tasks 2, 4
- Produces: Excalidraw-compatible `startBinding`/`endBinding`/`boundElements` in clipboard JSON

- [ ] **Step 1: Export bindings in convertToExcalidraw**

In `convertToExcalidraw`, after the line/arrow `points` insertion (after `elem.insert(QStringLiteral("points"), pts);`), add:

```cpp
    // Export bindings
    QVariantMap sb = shape.value(QStringLiteral("startBinding")).toMap();
    if (!sb.isEmpty()) {
        QJsonObject sbObj;
        sbObj.insert(QStringLiteral("elementId"), sb.value(QStringLiteral("elementId")).toString());
        sbObj.insert(QStringLiteral("focus"), sb.value(QStringLiteral("focus")).toDouble());
        sbObj.insert(QStringLiteral("gap"), sb.value(QStringLiteral("gap")).toDouble());
        elem.insert(QStringLiteral("startBinding"), sbObj);
    } else {
        elem.insert(QStringLiteral("startBinding"), QJsonValue::Null);
    }
    QVariantMap eb = shape.value(QStringLiteral("endBinding")).toMap();
    if (!eb.isEmpty()) {
        QJsonObject ebObj;
        ebObj.insert(QStringLiteral("elementId"), eb.value(QStringLiteral("elementId")).toString());
        ebObj.insert(QStringLiteral("focus"), eb.value(QStringLiteral("focus")).toDouble());
        ebObj.insert(QStringLiteral("gap"), eb.value(QStringLiteral("gap")).toDouble());
        elem.insert(QStringLiteral("endBinding"), ebObj);
    } else {
        elem.insert(QStringLiteral("endBinding"), QJsonValue::Null);
    }
```

For rect/ellipse shapes, after their geometry block, export `boundElements`:

```cpp
    // Export bound elements
    QVariantList boundIds = shape.value(QStringLiteral("boundElementIds")).toList();
    if (!boundIds.isEmpty()) {
        QJsonArray boundArr;
        for (const QVariant &bid : boundIds) {
            QJsonObject be;
            be.insert(QStringLiteral("id"), bid.toString());
            be.insert(QStringLiteral("type"), QStringLiteral("arrow"));
            boundArr.append(be);
        }
        elem.insert(QStringLiteral("boundElements"), boundArr);
    } else {
        elem.insert(QStringLiteral("boundElements"), QJsonValue::Null);
    }
```

- [ ] **Step 2: Import bindings in convertFromExcalidraw**

In `convertFromExcalidraw`, in the line/arrow block (after `shape.insert(QStringLiteral("toY"), toY);`), add:

```cpp
    // Import bindings
    QJsonValue sbVal = elem.value(QStringLiteral("startBinding"));
    if (sbVal.isObject()) {
        QJsonObject sbObj = sbVal.toObject();
        QVariantMap sb;
        sb[QStringLiteral("elementId")] = sbObj.value(QStringLiteral("elementId")).toString();
        sb[QStringLiteral("focus")] = sbObj.value(QStringLiteral("focus")).toDouble();
        sb[QStringLiteral("gap")] = sbObj.value(QStringLiteral("gap")).toDouble(0.0);
        shape.insert(QStringLiteral("startBinding"), sb);
    }
    QJsonValue ebVal = elem.value(QStringLiteral("endBinding"));
    if (ebVal.isObject()) {
        QJsonObject ebObj = ebVal.toObject();
        QVariantMap eb;
        eb[QStringLiteral("elementId")] = ebObj.value(QStringLiteral("elementId")).toString();
        eb[QStringLiteral("focus")] = ebObj.value(QStringLiteral("focus")).toDouble();
        eb[QStringLiteral("gap")] = ebObj.value(QStringLiteral("gap")).toDouble(0.0);
        shape.insert(QStringLiteral("endBinding"), eb);
    }
```

For rect/ellipse, after geometry import:

```cpp
    // Import bound elements
    QJsonValue beVal = elem.value(QStringLiteral("boundElements"));
    if (beVal.isArray()) {
        QVariantList boundIds;
        for (const QJsonValue &v : beVal.toArray()) {
            QJsonObject be = v.toObject();
            boundIds.append(be.value(QStringLiteral("id")).toString());
        }
        if (!boundIds.isEmpty()) {
            shape.insert(QStringLiteral("boundElementIds"), boundIds);
        }
    }
```

- [ ] **Step 3: Write Excalidraw round-trip test**

```cpp
void ShapesModelTest::testExcalidrawBindingRoundtrip()
{
    OverlayController ctrl;

    // Create shapes with bindings
    QVariantMap rect;
    rect["type"] = "rectangle"; rect["id"] = "rect-ex";
    rect["x"] = 100.0; rect["y"] = 100.0;
    rect["width"] = 200.0; rect["height"] = 100.0;
    rect["selected"] = true; rect["locked"] = false;
    rect["color"] = "#e63946"; rect["strokeWidth"] = 2;
    rect["opacity"] = 1.0; rect["roughness"] = 1; rect["seed"] = 12345;
    QVariantList boundIds;
    boundIds.append("arrow-ex");
    rect["boundElementIds"] = boundIds;
    ctrl.addShape(rect);

    QVariantMap arrow;
    arrow["type"] = "arrow"; arrow["id"] = "arrow-ex";
    arrow["fromX"] = 50.0; arrow["fromY"] = 50.0;
    arrow["toX"] = 200.0; arrow["toY"] = 100.0;
    arrow["selected"] = true; arrow["locked"] = false;
    arrow["color"] = "#e63946"; arrow["strokeWidth"] = 2;
    arrow["opacity"] = 1.0; arrow["roughness"] = 1; arrow["seed"] = 67890;
    QVariantMap endBinding;
    endBinding["elementId"] = "rect-ex";
    endBinding["focus"] = 0.0;
    endBinding["gap"] = 0.0;
    arrow["endBinding"] = endBinding;
    ctrl.addShape(arrow);

    // Copy (exports to Excalidraw clipboard format)
    ctrl.copySelected();

    // Clear and paste back
    ctrl.clear();
    QCOMPARE(ctrl.shapesModel()->rowCount(), 0);

    ctrl.pasteFromClipboard();

    // Verify bindings survived the round-trip
    // Find the arrow (should be pasted)
    bool foundArrow = false;
    for (int i = 0; i < ctrl.shapesModel()->rowCount(); ++i) {
        QVariantMap s = ctrl.getShape(i);
        if (s["type"].toString() == "arrow") {
            QVariantMap eb = s["endBinding"].toMap();
            // elementId should reference the pasted rect's id
            QVERIFY(!eb.isEmpty());
            QCOMPARE(eb["focus"].toDouble(), 0.0);
            foundArrow = true;
        }
    }
    QVERIFY(foundArrow);
}
```

Add `testExcalidrawBindingRoundtrip` to private Q_SLOTS.

- [ ] **Step 4: Build and run tests**

Run: `cd ~/coding/projects/scribbleway/build-deb && cmake .. && make -j$(nproc) && ctest --output-on-failure`

- [ ] **Step 5: Commit**

```bash
git add src/overlay/overlaycontroller.cpp tests/shapesmodeltest.cpp
git commit -m "feat: Excalidraw binding import/export compatibility"
```

---

### Task 9: Visual Snap Feedback During Endpoint Drag

When dragging an existing endpoint handle, show the same snap indicator and shape highlight as during drawing.

**Files:**
- Modify: `src/overlay/qml/shapes/BaseShape.qml:312-351` (endpoint handle drag — update snap state during drag)

**Interfaces:**
- Consumes: `findSnapInfo` from Task 7, `snapTargetIndex`/`snapPointX`/`snapPointY` from main.qml

- [ ] **Step 1: Update snap state during endpoint drag**

In `BaseShape.qml`, in the endpoint handle MouseArea `onPositionChanged`, add snap detection after updating the line geometry:

```qml
onPositionChanged: (mouse) => {
    if (pressed) {
        let pos = mapToItem(baseShapeRoot, mouse.x, mouse.y);
        let dx = pos.x - dragStartX;
        let dy = pos.y - dragStartY;
        let nx = originalX + dx;
        let ny = originalY + dy;
        if (index === 0) {
            lineGeometryChanged(nx, ny, shapeToX, shapeToY);
        } else {
            lineGeometryChanged(shapeFromX, shapeFromY, nx, ny);
        }

        // Update snap indicator
        if (typeof canvasWindow !== "undefined") {
            let snap = controller.findSnapInfo(nx, ny, shapeIndex);
            if (snap.valid) {
                canvasWindow.snapTargetIndex = snap.targetIndex;
                canvasWindow.snapPointX = snap.snapX;
                canvasWindow.snapPointY = snap.snapY;
            } else {
                canvasWindow.snapTargetIndex = -1;
            }
        }
    }
}
```

In `onReleased`, clear snap state:

```qml
onReleased: {
    if (typeof canvasWindow !== "undefined") {
        canvasWindow.snapTargetIndex = -1;
    }
    controller.resnapEndpoint(shapeIndex, index === 0);
    baseShapeRoot.isResizing = false;
    controller.endEdit();
}
```

- [ ] **Step 2: Build and smoke test**

Manual: draw a rect and an arrow bound to it. Select the arrow, drag the bound endpoint away (binding breaks, highlight disappears), drag it back near the rect edge (highlight and snap dot reappear), release (binding re-created).

- [ ] **Step 3: Commit**

```bash
git add src/overlay/qml/shapes/BaseShape.qml
git commit -m "feat: snap feedback during endpoint drag"
```
