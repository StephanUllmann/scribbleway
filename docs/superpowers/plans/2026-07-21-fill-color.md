# Fill Color Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give users control over rectangle/ellipse fill color and fill opacity (including “no fill”), replacing the hardcoded 12% stroke-color fill, with full Excalidraw `backgroundColor` round-trip.

**Architecture:** Store per-shape `fillColor` (hex string or `"transparent"`) and `fillOpacity` (0.0–1.0) in `ShapesModel`. Mirror defaults on `OverlayController` like glow/color. Render via BaseShape aliases in rectangle/ellipse QML and live previews. Expose controls through D-Bus/`AppletBackend` into the Plasma applet. Map to/from Excalidraw `backgroundColor` (encode alpha into RGBA hex when needed; `"transparent"` = no fill).

**Tech Stack:** Qt 6, C++20, QML, D-Bus, Kirigami, Qt Test.

## Global Constraints

* **Fill applies only to rectangle and ellipse** (not line/arrow/freehand/text).
* **Default new Scribbleway shapes:** `fillColor` = stroke color (`defaultColor`), `fillOpacity` = `0.12` (preserves current visual default).
* **No fill:** `fillColor == "transparent"` OR `fillOpacity == 0.0` → transparent fill.
* **Excalidraw paste:** read `backgroundColor`; `"transparent"` / missing → no fill (`fillColor="transparent"`, `fillOpacity=0.0`). Opaque hex → that color at `fillOpacity=1.0`. RGBA hex `#RRGGBBAA` → color + alpha.
* **Excalidraw copy:** if no fill → `backgroundColor="transparent"`. Else if `fillOpacity >= 0.999` → `#RRGGBB`. Else → `#RRGGBBAA` (alpha byte from `fillOpacity`).
* **`fillStyle`:** always emit `"solid"` on export (no hachure/cross-hatch in this plan). On import, ignore `fillStyle` value except that background still drives fill.
* **Keys:** QVariantMap / role names: `"fillColor"` (QString), `"fillOpacity"` (double).
* **Applet UI visibility:** fill controls visible when rectangle or ellipse tool/selection is active (same pattern as `isRectActive` for border radius).

---

## File Structure & Decomposition

* `src/overlay/shapesmodel.h` / `.cpp` — `FillColorRole`, `FillOpacityRole`
* `src/overlay/overlaycontroller.h` / `.cpp` — `defaultFillColor`, `defaultFillOpacity`; selection state; `updateProperties`; Excalidraw convert
* `src/overlay/qml/shapes/BaseShape.qml` — `modelFillColor`, `modelFillOpacity`
* `src/overlay/qml/shapes/RectangleShape.qml`, `EllipseShape.qml` — use model fill instead of hardcoded 0.12
* `src/overlay/qml/main.qml` — stamp fill on create; live preview fill
* `src/applet-plugin/appletbackend.h` / `.cpp` — `selectedFillColor`, `selectedFillOpacity`, setters
* `applet/contents/ui/FullRepresentation.qml` — fill swatches + opacity slider + no-fill control
* `tests/shapesmodeltest.cpp` — model/controller/Excalidraw/applet fill tests

---

### Task 1: Data Model Roles

**Files:**
- Modify: `src/overlay/shapesmodel.h:14-37`
- Modify: `src/overlay/shapesmodel.cpp:37-97,160-216`
- Test: `tests/shapesmodeltest.cpp` (compiled later with controller tests)

**Interfaces:**
- Consumes: none
- Produces: `ShapesModel::FillColorRole` → `"fillColor"`; `ShapesModel::FillOpacityRole` → `"fillOpacity"`; `data()` / `roleNames()` / `updateShape()` mappings

- [ ] **Step 1: Add roles to enum**

In `src/overlay/shapesmodel.h`, append after `GlowRole`:

```cpp
        SeedRole,
        GlowRole,
        FillColorRole,
        FillOpacityRole
    };
```

- [ ] **Step 2: Map roles in `data()`, `roleNames()`, `updateShape()`**

In `src/overlay/shapesmodel.cpp` `data()` after `GlowRole`:

```cpp
        case GlowRole: return shape.value(QStringLiteral("glow"));
        case FillColorRole: return shape.value(QStringLiteral("fillColor"));
        case FillOpacityRole: return shape.value(QStringLiteral("fillOpacity"));
        default: return QVariant();
```

In `roleNames()`:

```cpp
    roles[GlowRole] = "glow";
    roles[FillColorRole] = "fillColor";
    roles[FillOpacityRole] = "fillOpacity";
    return roles;
```

In `updateShape()` key→role map:

```cpp
                else if (it.key() == QStringLiteral("glow")) changedRoles << GlowRole;
                else if (it.key() == QStringLiteral("fillColor")) changedRoles << FillColorRole;
                else if (it.key() == QStringLiteral("fillOpacity")) changedRoles << FillOpacityRole;
```

- [ ] **Step 3: Commit**

```bash
git add src/overlay/shapesmodel.h src/overlay/shapesmodel.cpp
git commit -m "feat(model): add fillColor and fillOpacity shape roles"
```

---

### Task 2: OverlayController Defaults, Selection, Excalidraw

**Files:**
- Modify: `src/overlay/overlaycontroller.h:37-47,61-82,137-147,177-186`
- Modify: `src/overlay/overlaycontroller.cpp:31-36,157-259,305-335,374-437,1100-1255`
- Test: `tests/shapesmodeltest.cpp`

**Interfaces:**
- Consumes: `FillColorRole` / `FillOpacityRole` from Task 1
- Produces:
  - `Q_PROPERTY(QString defaultFillColor READ defaultFillColor WRITE setDefaultFillColor NOTIFY defaultFillColorChanged)`
  - `Q_PROPERTY(double defaultFillOpacity READ defaultFillOpacity WRITE setDefaultFillOpacity NOTIFY defaultFillOpacityChanged)`
  - Selection state keys `"fillColor"`, `"fillOpacity"`
  - `updateProperties` accepts those keys
  - `convertToExcalidraw` / `convertFromExcalidraw` map `backgroundColor`

- [ ] **Step 1: Declare properties in header**

In `src/overlay/overlaycontroller.h` after `defaultGlow`:

```cpp
    Q_PROPERTY(int defaultGlow READ defaultGlow WRITE setDefaultGlow NOTIFY defaultGlowChanged)
    Q_PROPERTY(QString defaultFillColor READ defaultFillColor WRITE setDefaultFillColor NOTIFY defaultFillColorChanged)
    Q_PROPERTY(double defaultFillOpacity READ defaultFillOpacity WRITE setDefaultFillOpacity NOTIFY defaultFillOpacityChanged)
```

Public API (near other default getters/setters):

```cpp
    QString defaultFillColor() const;
    void setDefaultFillColor(const QString &color);
    double defaultFillOpacity() const;
    void setDefaultFillOpacity(double opacity);
```

Signals:

```cpp
    void defaultGlowChanged();
    void defaultFillColorChanged();
    void defaultFillOpacityChanged();
```

Members (defaults match current hard-coded look: fill = stroke color, 12%):

```cpp
    int m_defaultGlow = 10;
    QString m_defaultFillColor;   // set in ctor to match m_defaultColor
    double m_defaultFillOpacity = 0.12;
```

- [ ] **Step 2: Constructor init**

In `OverlayController::OverlayController`, after `m_defaultColor` init:

```cpp
OverlayController::OverlayController(QObject *parent)
    : QObject(parent)
    , m_activeTool(QStringLiteral("freehand"))
    , m_defaultColor(QStringLiteral("#e63946"))
    , m_defaultFillColor(QStringLiteral("#e63946"))
{
    m_defaultFontFamily = QStringLiteral("monospace");
    // ...
}
```

- [ ] **Step 3: Implement getters/setters**

After `setDefaultGlow` in `overlaycontroller.cpp`:

```cpp
QString OverlayController::defaultFillColor() const
{
    return m_defaultFillColor;
}

void OverlayController::setDefaultFillColor(const QString &color)
{
    if (m_defaultFillColor != color) {
        m_defaultFillColor = color;
        Q_EMIT defaultFillColorChanged();
    }
}

double OverlayController::defaultFillOpacity() const
{
    return m_defaultFillOpacity;
}

void OverlayController::setDefaultFillOpacity(double opacity)
{
    const double clamped = qBound(0.0, opacity, 1.0);
    if (!qFuzzyCompare(m_defaultFillOpacity, clamped)) {
        m_defaultFillOpacity = clamped;
        Q_EMIT defaultFillOpacityChanged();
    }
}
```

- [ ] **Step 4: Sync defaults from selected shape**

In `setSelectedIndex` when applying selected shape properties (after glow block ~325):

```cpp
            if (shape.contains(QStringLiteral("glow"))) {
                m_defaultGlow = shape[QStringLiteral("glow")].toInt();
            }
            if (shape.contains(QStringLiteral("fillColor"))) {
                m_defaultFillColor = shape[QStringLiteral("fillColor")].toString();
            }
            if (shape.contains(QStringLiteral("fillOpacity"))) {
                m_defaultFillOpacity = shape[QStringLiteral("fillOpacity")].toDouble();
            }
            Q_EMIT defaultColorChanged();
            // ... existing emits ...
            Q_EMIT defaultGlowChanged();
            Q_EMIT defaultFillColorChanged();
            Q_EMIT defaultFillOpacityChanged();
```

- [ ] **Step 5: `getSelectionState()`**

Selected branch after glow:

```cpp
        state[QStringLiteral("glow")] = shape.value(QStringLiteral("glow"), m_defaultGlow).toInt();
        state[QStringLiteral("fillColor")] = shape.value(QStringLiteral("fillColor"), m_defaultFillColor).toString();
        state[QStringLiteral("fillOpacity")] = shape.value(QStringLiteral("fillOpacity"), m_defaultFillOpacity).toDouble();
```

No-selection branch:

```cpp
        state[QStringLiteral("glow")] = m_defaultGlow;
        state[QStringLiteral("fillColor")] = m_defaultFillColor;
        state[QStringLiteral("fillOpacity")] = m_defaultFillOpacity;
        state[QStringLiteral("locked")] = false;
```

- [ ] **Step 6: `updateProperties()`**

After glow handling:

```cpp
    if (demarshalled.contains(QStringLiteral("glow"))) {
        setDefaultGlow(demarshalled[QStringLiteral("glow")].toInt());
    }
    if (demarshalled.contains(QStringLiteral("fillColor"))) {
        setDefaultFillColor(demarshalled[QStringLiteral("fillColor")].toString());
    }
    if (demarshalled.contains(QStringLiteral("fillOpacity"))) {
        setDefaultFillOpacity(demarshalled[QStringLiteral("fillOpacity")].toDouble());
    }
```

(Existing loop already pushes the whole `demarshalled` map onto selected shapes — no extra per-shape write needed.)

- [ ] **Step 7: Keep stroke color and fill color independent when cycling stroke**

Do **not** auto-sync `m_defaultFillColor` when `setDefaultColor` / `cycleColor` / `selectPresetColor` runs. Users may want different stroke vs fill. New shapes still get whatever `defaultFillColor` currently is (initially same as stroke).

Optional UX (only if product wants it later, **out of scope**): a “match stroke” button in the applet.

- [ ] **Step 8: `convertToExcalidraw` — write `backgroundColor`**

Replace the hardcoded transparent background (~1122) with:

```cpp
    // Fill → Excalidraw backgroundColor
    {
        const QString fillColor = shape.value(QStringLiteral("fillColor"), QStringLiteral("transparent")).toString();
        const double fillOpacity = shape.value(QStringLiteral("fillOpacity"), 0.0).toDouble();
        const bool noFill = fillColor.isEmpty()
            || fillColor.compare(QStringLiteral("transparent"), Qt::CaseInsensitive) == 0
            || fillOpacity <= 0.0;

        if (noFill) {
            elem.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
        } else {
            QColor c(fillColor);
            if (!c.isValid()) {
                elem.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
            } else if (fillOpacity >= 0.999) {
                elem.insert(QStringLiteral("backgroundColor"), c.name(QColor::HexRgb)); // #RRGGBB
            } else {
                c.setAlphaF(fillOpacity);
                // Qt HexArgb is #AARRGGBB; Excalidraw expects #RRGGBBAA
                const QString argb = c.name(QColor::HexArgb); // #AARRGGBB
                const QString rrggbbaa = QStringLiteral("#%1%2")
                    .arg(argb.mid(3, 6))   // RRGGBB
                    .arg(argb.mid(1, 2));  // AA
                elem.insert(QStringLiteral("backgroundColor"), rrggbbaa);
            }
        }
    }
    elem.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
```

- [ ] **Step 9: `convertFromExcalidraw` — read `backgroundColor`**

After opacity/stroke handling (~1249), before geometry:

```cpp
    {
        const QString bg = elem.value(QStringLiteral("backgroundColor")).toString(QStringLiteral("transparent"));
        if (bg.isEmpty() || bg.compare(QStringLiteral("transparent"), Qt::CaseInsensitive) == 0) {
            shape.insert(QStringLiteral("fillColor"), QStringLiteral("transparent"));
            shape.insert(QStringLiteral("fillOpacity"), 0.0);
        } else {
            QString hex = bg;
            double fillOp = 1.0;
            // Excalidraw RGBA: #RRGGBBAA (9 chars with '#')
            if (hex.startsWith(QLatin1Char('#')) && hex.size() == 9) {
                bool ok = false;
                const int alphaByte = hex.mid(7, 2).toInt(&ok, 16);
                if (ok) {
                    fillOp = alphaByte / 255.0;
                }
                hex = hex.left(7); // #RRGGBB
            } else if (hex.startsWith(QLatin1Char('#')) && hex.size() == 7) {
                fillOp = 1.0;
            }
            // Guard white-on-white: leave fill as-is (stroke already remaps pure white)
            shape.insert(QStringLiteral("fillColor"), hex);
            shape.insert(QStringLiteral("fillOpacity"), fillOp);
        }
    }
```

- [ ] **Step 10: Write failing tests, then run them**

In `tests/shapesmodeltest.cpp`:

1. Add slot declaration after `testGlow()`:

```cpp
    void testGlow();
    void testFillColor();
    void testExcalidrawPasteCompatibility();
```

2. Implement `testFillColor` after `testGlow`:

```cpp
void ShapesModelTest::testFillColor()
{
    OverlayController controller;

    // Defaults preserve previous 12% stroke-colored fill look
    QCOMPARE(controller.defaultFillColor(), QStringLiteral("#e63946"));
    QCOMPARE(controller.defaultFillOpacity(), 0.12);

    QVariantMap defaults;
    defaults[QStringLiteral("fillColor")] = QStringLiteral("#00ff00");
    defaults[QStringLiteral("fillOpacity")] = 0.5;
    controller.updateProperties(defaults);
    QCOMPARE(controller.defaultFillColor(), QStringLiteral("#00ff00"));
    QCOMPARE(controller.defaultFillOpacity(), 0.5);

    // No selection state exposes defaults
    QVariantMap noSel = controller.getSelectionState();
    QCOMPARE(noSel[QStringLiteral("fillColor")].toString(), QStringLiteral("#00ff00"));
    QCOMPARE(noSel[QStringLiteral("fillOpacity")].toDouble(), 0.5);

    // Shape inherits explicit fill fields via add + selection
    QVariantMap shape;
    shape[QStringLiteral("type")] = QStringLiteral("rectangle");
    shape[QStringLiteral("fillColor")] = QStringLiteral("#112233");
    shape[QStringLiteral("fillOpacity")] = 0.25;
    controller.addShape(shape);

    QVariantMap sel = controller.getSelectionState();
    QCOMPARE(sel[QStringLiteral("fillColor")].toString(), QStringLiteral("#112233"));
    QCOMPARE(sel[QStringLiteral("fillOpacity")].toDouble(), 0.25);
    QCOMPARE(controller.defaultFillColor(), QStringLiteral("#112233"));
    QCOMPARE(controller.defaultFillOpacity(), 0.25);

    // updateProperties mutates selected shape + defaults
    QVariantMap upd;
    upd[QStringLiteral("fillColor")] = QStringLiteral("transparent");
    upd[QStringLiteral("fillOpacity")] = 0.0;
    controller.updateProperties(upd);
    QCOMPARE(controller.shapesModel()->shapes().first()[QStringLiteral("fillColor")].toString(),
             QStringLiteral("transparent"));
    QCOMPARE(controller.shapesModel()->shapes().first()[QStringLiteral("fillOpacity")].toDouble(), 0.0);

    // Role names surface to QML
    ShapesModel *model = controller.shapesModel();
    const QModelIndex idx = model->index(0, 0);
    const QHash<int, QByteArray> roles = model->roleNames();
    QVERIFY(roles.values().contains("fillColor"));
    QVERIFY(roles.values().contains("fillOpacity"));
    QCOMPARE(model->data(idx, ShapesModel::FillColorRole).toString(), QStringLiteral("transparent"));
    QCOMPARE(model->data(idx, ShapesModel::FillOpacityRole).toDouble(), 0.0);

    // Clamp opacity
    controller.setDefaultFillOpacity(1.5);
    QCOMPARE(controller.defaultFillOpacity(), 1.0);
    controller.setDefaultFillOpacity(-0.2);
    QCOMPARE(controller.defaultFillOpacity(), 0.0);
}
```

3. Extend `testExcalidrawPasteCompatibility` rectangle element to include background, and assert after paste:

In the rect JSON builder (~630), add:

```cpp
    rectObj.insert(QStringLiteral("backgroundColor"), QStringLiteral("#00ff0080")); // 50% green
```

After paste assertions for the rectangle (~718):

```cpp
    QCOMPARE(pastedRect[QStringLiteral("fillColor")].toString(), QStringLiteral("#00ff00"));
    QCOMPARE(pastedRect[QStringLiteral("fillOpacity")].toDouble(), 128.0 / 255.0);
```

Also add a second small unit inside `testFillColor` (or extend `testOverlayControllerCopyPaste` if preferred) for export:

```cpp
    // Round-trip export encoding
    QVariantMap exportShape;
    exportShape[QStringLiteral("type")] = QStringLiteral("ellipse");
    exportShape[QStringLiteral("color")] = QStringLiteral("#ff0000");
    exportShape[QStringLiteral("strokeWidth")] = 2;
    exportShape[QStringLiteral("opacity")] = 1.0;
    exportShape[QStringLiteral("x")] = 0;
    exportShape[QStringLiteral("y")] = 0;
    exportShape[QStringLiteral("width")] = 10;
    exportShape[QStringLiteral("height")] = 10;
    exportShape[QStringLiteral("fillColor")] = QStringLiteral("#0000ff");
    exportShape[QStringLiteral("fillOpacity")] = 0.5;
    // Use public copy path: add, select, copy, inspect clipboard JSON
    controller.clear();
    controller.addShape(exportShape);
    controller.copySelected();
    const QString clip = QGuiApplication::clipboard()->text();
    const QJsonObject root = QJsonDocument::fromJson(clip.toUtf8()).object();
    const QJsonArray elements = root.value(QStringLiteral("elements")).toArray();
    QVERIFY(!elements.isEmpty());
    const QString bg = elements.at(0).toObject().value(QStringLiteral("backgroundColor")).toString();
    // Expect #0000ff80 (approx 0.5 * 255 = 127.5 → 127 or 128 depending on rounding)
    QVERIFY(bg.startsWith(QStringLiteral("#0000ff")));
    QCOMPARE(bg.size(), 9);
```

If `copySelected` wraps elements differently, match the existing `testOverlayControllerCopyPaste` JSON shape exactly.

- [ ] **Step 11: Build and run tests**

```bash
cmake --build build --target shapesmodeltest
ctest --test-dir build -R shapesmodeltest --output-on-failure
```

Expected: `testFillColor` and updated Excalidraw test PASS.

- [ ] **Step 12: Commit**

```bash
git add src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp tests/shapesmodeltest.cpp
git commit -m "feat(controller): default fill color/opacity and Excalidraw backgroundColor"
```

---

### Task 3: QML Rendering & Live Preview

**Files:**
- Modify: `src/overlay/qml/shapes/BaseShape.qml:36-42`
- Modify: `src/overlay/qml/shapes/RectangleShape.qml:18-33`
- Modify: `src/overlay/qml/shapes/EllipseShape.qml:18-28`
- Modify: `src/overlay/qml/main.qml:120-130,313-342`

**Interfaces:**
- Consumes: model roles `fillColor`/`fillOpacity`; `controller.defaultFillColor` / `defaultFillOpacity`
- Produces: visible fill for rect/ellipse + matching live previews

- [ ] **Step 1: BaseShape aliases**

In `BaseShape.qml` after `modelGlow`:

```qml
    property int modelGlow: model.glow !== undefined ? model.glow : 0
    property int modelSeed: model.seed !== undefined ? model.seed : 123456
    property string modelFillColor: model.fillColor !== undefined ? model.fillColor : "transparent"
    property real modelFillOpacity: model.fillOpacity !== undefined ? model.fillOpacity : 0.0
```

Add a shared computed fill used by children (optional helper on BaseShape):

```qml
    readonly property color resolvedFill: {
        if (!modelFillColor || modelFillColor === "transparent" || modelFillOpacity <= 0)
            return "transparent";
        let c = Qt.color(modelFillColor);
        return Qt.rgba(c.r, c.g, c.b, modelFillOpacity);
    }
```

- [ ] **Step 2: RectangleShape**

Replace hardcoded fill (~27-31):

```qml
        border.color: root.modelColor
        border.width: root.modelRoughness === 0 ? root.modelStrokeWidth : 0
        color: root.resolvedFill
        radius: typeof borderRadius !== "undefined" ? borderRadius : 0
```

Note: rough mode still only draws stroke via `RoughStroke` (fill stays under the sketchy stroke as a plain rect). That matches current architecture where roughness is stroke-only — keep it.

- [ ] **Step 3: EllipseShape**

Replace `fillColor` block (~25-28):

```qml
                fillColor: root.resolvedFill
```

- [ ] **Step 4: Stamp fill on shape creation in `main.qml`**

In `finalizeShape()` shape literal (~120-130):

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
            "fillColor": controller.defaultFillColor,
            "fillOpacity": controller.defaultFillOpacity,
            "seed": Math.floor(Math.random() * 1000000) + 1
        };
```

Fill on non-rect/ellipse shapes is harmless (unused by their QML). Prefer always stamping for simpler code and clipboard consistency.

- [ ] **Step 5: Live previews**

Rectangle preview (~323-326):

```qml
        color: {
            let fc = controller.defaultFillColor;
            let fo = controller.defaultFillOpacity;
            if (!fc || fc === "transparent" || fo <= 0)
                return "transparent";
            let c = Qt.color(fc);
            return Qt.rgba(c.r, c.g, c.b, fo);
        }
```

Ellipse preview `fillColor` (~339-342): same expression.

- [ ] **Step 6: Manual smoke (no automated QML test harness)**

Build overlay:

```bash
cmake --build build --target scribbleway-overlay
```

Manually: draw rectangle/ellipse → expect ~12% red fill; change fill via upcoming applet (or temporary `controller.defaultFillOpacity = 0` in QML console if available) → preview updates.

- [ ] **Step 7: Commit**

```bash
git add src/overlay/qml/shapes/BaseShape.qml \
        src/overlay/qml/shapes/RectangleShape.qml \
        src/overlay/qml/shapes/EllipseShape.qml \
        src/overlay/qml/main.qml
git commit -m "feat(qml): render configurable fill on rect/ellipse and previews"
```

---

### Task 4: AppletBackend D-Bus Bridge

**Files:**
- Modify: `src/applet-plugin/appletbackend.h:16-25,37-47,57-64,105-112`
- Modify: `src/applet-plugin/appletbackend.cpp:58-107,125-158,349-363`
- Test: extend `testAppletBackendIntegration` in `tests/shapesmodeltest.cpp` if D-Bus path is exercised there

**Interfaces:**
- Consumes: selection state `fillColor`/`fillOpacity`; `OverlayController::updateProperties`
- Produces:
  - `Q_PROPERTY(QString selectedFillColor READ selectedFillColor NOTIFY selectionChanged)`
  - `Q_PROPERTY(double selectedFillOpacity READ selectedFillOpacity NOTIFY selectionChanged)`
  - `Q_INVOKABLE void setFillColor(const QString &color)`
  - `Q_INVOKABLE void setFillOpacity(double opacity)`

- [ ] **Step 1: Header declarations**

```cpp
    Q_PROPERTY(int selectedGlow READ selectedGlow NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedFillColor READ selectedFillColor NOTIFY selectionChanged)
    Q_PROPERTY(double selectedFillOpacity READ selectedFillOpacity NOTIFY selectionChanged)
```

Getters:

```cpp
    int selectedGlow() const;
    QString selectedFillColor() const;
    double selectedFillOpacity() const;
```

Invokables:

```cpp
    Q_INVOKABLE void setGlow(int glow);
    Q_INVOKABLE void setFillColor(const QString &color);
    Q_INVOKABLE void setFillOpacity(double opacity);
```

Members:

```cpp
    int m_selectedGlow = 10;
    QString m_selectedFillColor = QStringLiteral("#e63946");
    double m_selectedFillOpacity = 0.12;
```

- [ ] **Step 2: Implement in cpp**

Getters next to `selectedGlow()`:

```cpp
QString AppletBackend::selectedFillColor() const
{
    return m_selectedFillColor;
}

double AppletBackend::selectedFillOpacity() const
{
    return m_selectedFillOpacity;
}
```

Setters next to `setGlow`:

```cpp
void AppletBackend::setFillColor(const QString &color)
{
    sendDBus(QStringLiteral("updateProperties"),
             { QVariantMap{{QStringLiteral("fillColor"), color}} });
}

void AppletBackend::setFillOpacity(double opacity)
{
    sendDBus(QStringLiteral("updateProperties"),
             { QVariantMap{{QStringLiteral("fillOpacity"), opacity}} });
}
```

In `onSelectionChanged` after glow:

```cpp
    m_selectedGlow = demarshalled.value(QStringLiteral("glow"), 3).toInt();
    m_selectedFillColor = demarshalled.value(QStringLiteral("fillColor"), QStringLiteral("#e63946")).toString();
    m_selectedFillOpacity = demarshalled.value(QStringLiteral("fillOpacity"), 0.12).toDouble();
    Q_EMIT selectionChanged();
```

- [ ] **Step 3: Optional integration assert**

In `testAppletBackendIntegration` after `setOpacity` checks (~1235), if the test process has a live controller on D-Bus:

```cpp
    backend.setFillColor(QStringLiteral("#abcdef"));
    backend.setFillOpacity(0.33);
    QTest::qWait(50);
    QCOMPARE(controller.defaultFillColor(), QStringLiteral("#abcdef"));
    QCOMPARE(controller.defaultFillOpacity(), 0.33);
```

Only add if that test already drives the same controller instance via D-Bus (it does for color/width/opacity). Mirror that pattern exactly.

- [ ] **Step 4: Build plugin + tests**

```bash
cmake --build build --target scribblewaybackend shapesmodeltest
ctest --test-dir build -R shapesmodeltest --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add src/applet-plugin/appletbackend.h src/applet-plugin/appletbackend.cpp tests/shapesmodeltest.cpp
git commit -m "feat(applet): expose fill color and fill opacity over D-Bus"
```

---

### Task 5: Plasma Applet UI

**Files:**
- Modify: `applet/contents/ui/FullRepresentation.qml:17-30,245-367`

**Interfaces:**
- Consumes: `backend.selectedFillColor`, `selectedFillOpacity`, `setFillColor`, `setFillOpacity`
- Produces: Fill color row + fill opacity slider, visible for rect/ellipse

- [ ] **Step 1: Active-tool helper**

Near `isRectActive` (~29):

```qml
    property bool isRectActive: (root.backend.hasSelection && root.backend.selectedType.toLowerCase() === "rectangle") || (!root.backend.hasSelection && currentToolName === "rectangle")
    property bool isFillableActive: {
        const t = root.backend.hasSelection
            ? root.backend.selectedType.toLowerCase()
            : fullRoot.currentToolName;
        return t === "rectangle" || t === "ellipse";
    }
```

- [ ] **Step 2: Second ColorDialog for fill (or reuse with mode flag)**

Prefer a dedicated dialog to avoid fighting stroke color dialog state:

```qml
    ColorDialog {
        id: colorDialog
        title: "Choose Custom Color"
        onAccepted: {
            root.backend.setColor(colorDialog.selectedColor.toString())
        }
    }

    ColorDialog {
        id: fillColorDialog
        title: "Choose Fill Color"
        onAccepted: {
            root.backend.setFillColor(fillColorDialog.selectedColor.toString())
        }
    }
```

- [ ] **Step 3: Fill controls UI**

Insert **after** the stroke Opacity row and **before** Glow (or after Glow — either is fine; prefer after stroke Opacity so stroke then fill group together), gated on `isFillableActive`:

```qml
        // Fill Color (rectangle / ellipse only)
        ColumnLayout {
            Layout.fillWidth: true
            visible: fullRoot.isFillableActive
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.Label {
                    text: "Fill:"
                    Layout.alignment: Qt.AlignVCenter
                    width: Kirigami.Units.gridUnit * 3
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    property var colors: ["#e63946", "#f4a261", "#e9c46a", "#2a9d8f", "#457b9d", "#8338ec"]
                    property string activeFill: root.backend.selectedFillColor

                    // No-fill swatch
                    Rectangle {
                        width: Kirigami.Units.gridUnit * 1.5
                        height: Kirigami.Units.gridUnit * 1.5
                        radius: 4
                        color: Kirigami.Theme.backgroundColor
                        border.width: parent.activeFill === "transparent" ? 2 : 1
                        border.color: parent.activeFill === "transparent" ? Kirigami.Theme.highlightColor : "gray"

                        // Simple X to indicate no fill
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width * 0.7
                            height: 2
                            rotation: 45
                            color: "gray"
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width * 0.7
                            height: 2
                            rotation: -45
                            color: "gray"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                root.backend.setFillColor("transparent")
                                root.backend.setFillOpacity(0)
                            }
                        }
                    }

                    Repeater {
                        model: parent.colors
                        Rectangle {
                            width: Kirigami.Units.gridUnit * 1.5
                            height: Kirigami.Units.gridUnit * 1.5
                            radius: 4
                            color: modelData
                            border.width: parent.activeFill === modelData ? 2 : 1
                            border.color: parent.activeFill === modelData ? Kirigami.Theme.highlightColor : "gray"

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.backend.setFillColor(modelData)
                                    // If user picks a color while no-fill, restore a visible opacity
                                    if (root.backend.selectedFillOpacity <= 0)
                                        root.backend.setFillOpacity(0.12)
                                }
                            }
                        }
                    }

                    PlasmaComponents.Button {
                        icon.name: "color-picker"
                        text: "Custom"
                        onClicked: {
                            const cur = root.backend.selectedFillColor
                            fillColorDialog.selectedColor = (cur && cur !== "transparent") ? cur : root.backend.selectedColor
                            fillColorDialog.open()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                PlasmaComponents.Label {
                    text: "Fill Op:"
                    width: Kirigami.Units.gridUnit * 3
                }

                PlasmaComponents.Slider {
                    Layout.fillWidth: true
                    from: 0.0
                    to: 1.0
                    stepSize: 0.05
                    value: root.backend.selectedFillOpacity
                    onMoved: {
                        root.backend.setFillOpacity(value)
                        // Sliding up from zero while transparent: adopt stroke color as fill
                        if (value > 0 && root.backend.selectedFillColor === "transparent")
                            root.backend.setFillColor(root.backend.selectedColor)
                    }
                }

                PlasmaComponents.Label {
                    text: Math.round(root.backend.selectedFillOpacity * 100) + "%"
                }
            }
        }
```

- [ ] **Step 4: Manual UI verification**

1. Build/install applet plugin as usual for local dev.
2. Select rectangle tool → Fill row visible; line tool → hidden.
3. No-fill swatch → live preview transparent inside stroke.
4. Pick green fill + 50% → preview/shape update via D-Bus.
5. Select existing ellipse → sliders reflect its fill.
6. Stroke color change does **not** force fill color change.

- [ ] **Step 5: Commit**

```bash
git add applet/contents/ui/FullRepresentation.qml
git commit -m "feat(applet-ui): fill color swatches and fill opacity slider"
```

---

### Task 6: Final Verification & Acceptance

**Files:** none new — regression pass

- [ ] **Step 1: Full test suite**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests PASS, including `testFillColor` and Excalidraw paste with `backgroundColor`.

- [ ] **Step 2: Acceptance checklist**

| # | Criterion | Pass? |
|---|-----------|-------|
| 1 | New rect/ellipse default fill matches prior look (stroke color @ 12%) | |
| 2 | `fillColor` / `fillOpacity` stored on shape maps and exposed as model roles | |
| 3 | Applet can set fill color, fill opacity, and no-fill on selection and as defaults | |
| 4 | Live preview uses the same fill defaults while drawing | |
| 5 | Roughness still stroke-only; fill remains solid under rough stroke | |
| 6 | Copy to Excalidraw writes correct `backgroundColor` (`transparent` / `#RRGGBB` / `#RRGGBBAA`) | |
| 7 | Paste from Excalidraw restores fill color + opacity | |
| 8 | Line/arrow/freehand/text ignore fill for rendering; fill UI hidden unless rect/ellipse | |
| 9 | Undo after fill change restores previous fill | |
| 10 | No regressions in glow/roughness/border radius/tests | |

- [ ] **Step 3: Final commit only if leftover fixups**

```bash
git status
# commit any stray fixes
```

---

## Excalidraw Compatibility Impact

| Direction | Field | Behavior |
|-----------|-------|----------|
| Export | `backgroundColor` | From `fillColor`+`fillOpacity`; `transparent` if no fill |
| Export | `fillStyle` | Always `"solid"` |
| Import | `backgroundColor` | Drives `fillColor`/`fillOpacity`; supports `#RRGGBB` and `#RRGGBBAA` |
| Import | `fillStyle` | Ignored (hachure not implemented) |
| Non-fill shapes | — | Still export `backgroundColor: "transparent"` when fill unused/empty |

Scribbleway-only keys `fillColor` / `fillOpacity` are **not** written as exotic Excalidraw fields; they only map through `backgroundColor`.

---

## Design Notes (implementer)

1. **Why two fields instead of one RGBA string?** Matches existing `color` + `opacity` stroke pattern, makes the opacity slider trivial, and keeps “no fill” a single transparent color without destroying the last chosen hue when opacity hits 0 (optional: applet may set both on no-fill click).
2. **Resolved fill in BaseShape** avoids duplicating the transparent/alpha math in every shape file.
3. **Rough fills:** Out of scope. RoughPathGenerator only produces strokes; solid fill underneath is acceptable and matches current 12% fill under rough stroke.
4. **`cycleColor` / presets** intentionally do not change fill — avoids surprising users who set a distinct fill.

---

## Self-Review

- Spec coverage: model roles, controller defaults, QML render/preview, D-Bus, applet UI, tests, Excalidraw — all tasked.
- No placeholders / TBDs in steps.
- Names consistent: `fillColor`, `fillOpacity`, `defaultFillColor`, `defaultFillOpacity`, `selectedFillColor`, `selectedFillOpacity`, `setFillColor`, `setFillOpacity`, `FillColorRole`, `FillOpacityRole`.
