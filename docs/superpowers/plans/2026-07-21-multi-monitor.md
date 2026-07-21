# Multi-Monitor Awareness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Scribbleway annotate every connected monitor at once, with one shared shape model and correct coordinates, instead of a single `Screen.width × Screen.height` overlay that jumps via `setTargetScreen`.

**Architecture:** Create **one LayerShell overlay window per `QScreen`**, all sharing the same `OverlayController` / `ShapesModel`. Store shape geometry in **virtual-desktop coordinates** (`QScreen::geometry()` space). Each window converts local mouse ↔ virtual on input, and virtual ↔ local on render via a per-window origin offset. Drop the single-target-screen model.

**Tech Stack:** Qt 6, C++20, QML, LayerShellQt 6.x (`wlr-layer-shell` per-output surfaces), D-Bus, Qt Test.

## Research Summary (LayerShellQt)

From `/usr/include/LayerShellQt/window.h` (v6.5.2):

| API | Relevance |
|-----|-----------|
| `ScreenConfiguration::ScreenFromQWindow` (default) | Surface is placed on `QWindow::screen()`. **Use this.** |
| `ScreenFromCompositor` | Compositor picks output — not useful for explicit multi-monitor. |
| `setAnchors(Top\|Bottom\|Left\|Right)` | Full-output coverage (already used in `main.cpp` ~107–112). |
| `setCloseOnDismissed(bool)` | If `false`, window survives output removal so we can remap/destroy ourselves. **Set `false`.** |
| `setScope` | Keep `"scribbleway-overlay"` (or per-screen `"scribbleway-overlay/<name>"` if compositor stacks by scope). |
| No multi-output span API | **A single layer-shell surface cannot span monitors.** Option (b) is not viable on Wayland. |

**Design choice: (a) one window per screen, shared model, virtual-desktop coordinates.**  
Option (b) (one window spanning the virtual desktop) is rejected: layer-shell is per-output; Qt Wayland will not give a reliable cross-output layer surface.

## Global Constraints

* All shape keys (`x`, `y`, `width`, `height`, `fromX`, `fromY`, `toX`, `toY`, freehand `points`) are stored in **virtual-desktop coordinates**.
* Every connected `QScreen` gets exactly one overlay `QQuickWindow` for its lifetime in the app.
* Hotplug: create on `screenAdded`, destroy on `screenRemoved`; update origins on `QScreen::geometryChanged`.
* Shared mode/tool/selection/undo across all screens (single `OverlayController`).
* Keyboard interactivity and input masks are applied to **all** overlay windows (simple, correct).
* Applet “Target Screen” combo is removed or replaced; D-Bus `setTargetScreen` becomes a no-op legacy slot (or removes the window jump).
* Excalidraw clipboard format unchanged — virtual coords are just a larger scene.
* Passthrough still uses a 1×1 local mask per window (existing Qt Wayland empty-mask pitfall in `updateInputMask` ~358–363).

## Current Code (ground truth)

| Location | Behavior |
|----------|----------|
| `src/overlay/main.cpp:100–123` | One window, `setScreen(primaryScreen)`, LayerShell full anchors, show |
| `src/overlay/qml/main.qml:11–12` | `width: Screen.width`, `height: Screen.height` |
| `src/overlay/overlaycontroller.h:32,128,171` | Single `QQuickWindow* window` property; `setTargetScreen` slot |
| `src/overlay/overlaycontroller.cpp:346–370` | `updateInputMask` → `m_window->setMask` |
| `src/overlay/overlaycontroller.cpp:489–516` | Keyboard interactivity + hide/setScreen/show target switch |
| `src/overlay/qml/main.qml:113–178` | `finalizeShape` stores **local** mouse coords |
| `src/overlay/qml/main.qml:214–238` | Repeater binds shapes at model coords (assumed local) |
| `src/applet-plugin/appletbackend.*` | `screenNames`, `targetScreen`, D-Bus forward |
| `applet/contents/ui/FullRepresentation.qml:218–243` | “Target Screen” ComboBox |
| `applet/contents/config/main.xml` | `targetScreen` KConfig entry |

---

## File Structure & Decomposition

* `src/overlay/main.cpp` — multi-window factory; screen hotplug hooks; LayerShell helper
* `src/overlay/overlaycontroller.h` / `.cpp` — multi-window registry, mask/keyboard fan-out, virtual-coord paste fallback, retire target-screen jump
* `src/overlay/qml/main.qml` — per-window `screenOriginX/Y` + `screenName`; local↔virtual at draw/select/paste; render offset for shapes
* Shape QML (`RectangleShape.qml` etc.) — only if geometry write-back needs origin (prefer fix at `main.qml` bindings / handlers)
* `src/applet-plugin/appletbackend.h` / `.cpp` — drop or gut `targetScreen` write path; keep `screenNames` as status
* `applet/contents/ui/FullRepresentation.qml` — remove Target Screen row (or show read-only screen list)
* `applet/contents/ui/main.qml` — stop calling `setTargetScreen` on startup
* `applet/contents/config/main.xml` — remove `targetScreen` (optional migration: leave unused)
* `tests/shapesmodeltest.cpp` — registry, mask fan-out, coord helpers, legacy D-Bus

**No new ShapeRoles / QVariantMap keys / Excalidraw fields.**  
**Optional new Q_PROPERTY:** `QStringList screenNames` on controller (for debugging/applet).  
**D-Bus:** keep `setTargetScreen(QString)` signature as no-op for one release to avoid breaking old applets; stop using it.

---

### Coordinate Convention

```
Virtual point V = (localX + screen.geometry().x(), localY + screen.geometry().y())
Local point  L = (virtualX - screen.geometry().x(), virtualY - screen.geometry().y())
```

`QScreen::geometry()` is already in virtual-desktop pixels (Qt GUI).  
Do **not** use `QCursor::pos()` mapped through a single `m_window` without choosing the screen under the cursor.

Per-window QML properties set from C++ before show:

```qml
property string screenName: ""      // QScreen::name()
property real screenOriginX: 0      // screen->geometry().x()
property real screenOriginY: 0      // screen->geometry().y()
```

---

### Task 1: Multi-window registry on OverlayController

**Files:**
* Modify: `src/overlay/overlaycontroller.h`
* Modify: `src/overlay/overlaycontroller.cpp`

**Interfaces:**
* Consumes: existing single `m_window` call sites (`updateInputMask`, `setKeyboardInteractivity`, `setTargetScreen`, `pasteFromClipboard` fallback, `setWindow`)
* Produces: multi-window API used by `main.cpp` and QML

- [ ] **Step 1: Replace single-window storage**

In `overlaycontroller.h`:

```cpp
// Remove or keep as "primary/first window" convenience:
// Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)

// Add:
Q_PROPERTY(QStringList overlayScreenNames READ overlayScreenNames NOTIFY windowsChanged)

public:
    QQuickWindow *window() const; // returns primary-associated or first window (compat)
    void setWindow(QQuickWindow *window); // legacy: register as sole/primary — prefer addOverlayWindow

    Q_INVOKABLE void addOverlayWindow(QQuickWindow *window, const QString &screenName);
    Q_INVOKABLE void removeOverlayWindow(const QString &screenName);
    Q_INVOKABLE void updateOverlayScreenGeometry(const QString &screenName, const QRect &geometry);
    QStringList overlayScreenNames() const;

    // Change updateInputMask to be per-window (QML knows its window):
    Q_INVOKABLE void updateInputMaskForScreen(const QString &screenName, const QVariantList &rects);
    // Keep updateInputMask(rects) as "apply same local rects to all windows" for passthrough full clear

Q_SIGNALS:
    void windowsChanged();
```

Private:

```cpp
struct OverlaySurface {
    QPointer<QQuickWindow> window;
    QString screenName;
    QRect screenGeometry; // virtual-desktop geometry last applied
    QRegion lastInputMask;
};
QList<OverlaySurface> m_surfaces;
// Remove: QQuickWindow *m_window; QRegion m_lastInputMask;
```

- [ ] **Step 2: Implement registry**

```cpp
void OverlayController::addOverlayWindow(QQuickWindow *window, const QString &screenName)
{
    if (!window || screenName.isEmpty()) return;
    for (auto &s : m_surfaces) {
        if (s.screenName == screenName) {
            s.window = window;
            Q_EMIT windowChanged();
            Q_EMIT windowsChanged();
            return;
        }
    }
    OverlaySurface surface;
    surface.window = window;
    surface.screenName = screenName;
    if (QScreen *sc = window->screen())
        surface.screenGeometry = sc->geometry();
    m_surfaces.append(surface);
    Q_EMIT windowChanged();
    Q_EMIT windowsChanged();
}

void OverlayController::removeOverlayWindow(const QString &screenName)
{
    for (int i = 0; i < m_surfaces.size(); ++i) {
        if (m_surfaces[i].screenName == screenName) {
            if (auto *w = m_surfaces[i].window.data()) {
                w->hide();
                w->deleteLater();
            }
            m_surfaces.removeAt(i);
            Q_EMIT windowChanged();
            Q_EMIT windowsChanged();
            return;
        }
    }
}
```

- [ ] **Step 3: Fan-out keyboard interactivity**

Replace `setKeyboardInteractivity` body (`overlaycontroller.cpp` ~489–497):

```cpp
void OverlayController::setKeyboardInteractivity(bool interactive)
{
    const auto mode = interactive
        ? LayerShellQt::Window::KeyboardInteractivityOnDemand
        : LayerShellQt::Window::KeyboardInteractivityNone;
    for (const OverlaySurface &s : m_surfaces) {
        if (!s.window) continue;
        if (auto *layerWindow = LayerShellQt::Window::get(s.window)) {
            layerWindow->setKeyboardInteractivity(mode);
        }
    }
}
```

- [ ] **Step 4: Per-screen input mask**

```cpp
void OverlayController::updateInputMaskForScreen(const QString &screenName, const QVariantList &rects)
{
    for (OverlaySurface &s : m_surfaces) {
        if (s.screenName != screenName || !s.window) continue;
        QRegion region;
        for (const QVariant &v : rects) {
            const QRect r = v.toRectF().toRect();
            if (r.isValid()) region += r;
        }
        if (region.isEmpty())
            region += QRect(0, 0, 1, 1); // Wayland empty-mask pitfall
        if (s.lastInputMask == region) return;
        s.lastInputMask = region;
        s.window->setMask(region);
        return;
    }
}

void OverlayController::updateInputMask(const QVariantList &rects)
{
    // Apply identical local rects to every surface (used for global passthrough clear / full capture).
    for (const OverlaySurface &s : m_surfaces) {
        if (!s.screenName.isEmpty())
            updateInputMaskForScreen(s.screenName, rects);
    }
}
```

- [ ] **Step 5: Retire setTargetScreen jump**

```cpp
void OverlayController::setTargetScreen(const QString &screenName)
{
    Q_UNUSED(screenName);
    // Legacy no-op: overlays cover all screens. Kept for D-Bus ABI with older applets.
}
```

- [ ] **Step 6: Fix paste fallback to virtual coords**

In `pasteFromClipboard` (`~986–995`), when `localX/localY` are not provided:

```cpp
    QPointF anchor;
    if (localX >= 0.0 && localY >= 0.0) {
        // Caller must pass VIRTUAL coords (QML converts before invoke).
        anchor = QPointF(localX, localY);
    } else {
        // QCursor::pos() is in virtual-desktop pixels on Qt platforms we care about.
        anchor = QPointF(QCursor::pos());
    }
    // use anchor instead of localMousePos for placement math below
```

Document: QML paste shortcut must pass virtual coords:
`controller.pasteFromClipboard(lastMousePos.x + screenOriginX, lastMousePos.y + screenOriginY)`.

- [ ] **Step 7: Compat `window()` getter**

```cpp
QQuickWindow *OverlayController::window() const
{
    for (const OverlaySurface &s : m_surfaces) {
        if (s.window) return s.window;
    }
    return nullptr;
}
```

`setWindow(w)` may call `addOverlayWindow(w, w->screen() ? w->screen()->name() : QStringLiteral("unknown"))` for tests that still use the old path.

- [ ] **Step 8: Build**

```bash
cmake --build build --target scribbleway-overlay
```

Expected: compiles; fix any remaining `m_window` / `m_lastInputMask` references via grep.

---

### Task 2: Window factory + LayerShell per screen in main.cpp

**Files:**
* Modify: `src/overlay/main.cpp`

**Interfaces:**
* Consumes: `OverlayController::addOverlayWindow` / `removeOverlayWindow` / `updateOverlayScreenGeometry`
* Produces: one configured LayerShell window per `QScreen`, hotplug-safe

- [ ] **Step 1: Extract LayerShell configure helper**

```cpp
static void configureLayerShell(QQuickWindow *window)
{
    auto *layerWindow = LayerShellQt::Window::get(window);
    if (!layerWindow)
        return;

    LayerShellQt::Window::Anchors anchors;
    anchors.setFlag(LayerShellQt::Window::AnchorTop);
    anchors.setFlag(LayerShellQt::Window::AnchorBottom);
    anchors.setFlag(LayerShellQt::Window::AnchorLeft);
    anchors.setFlag(LayerShellQt::Window::AnchorRight);
    layerWindow->setAnchors(anchors);
    layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    layerWindow->setExclusiveZone(0);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow->setScope(QStringLiteral("scribbleway-overlay"));
    layerWindow->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
    layerWindow->setCloseOnDismissed(false); // we own lifecycle on screen removal
}
```

- [ ] **Step 2: Create windows via QQmlComponent (not single engine.load root)**

Replace the single `engine.load` + `objectCreated` path with:

```cpp
engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);

QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/scribbleway/qml/main.qml")));
if (component.isError()) {
    qCritical() << component.errors();
    return -1;
}

const auto createOverlayForScreen = [&](QScreen *screen) {
    if (!screen) return;

    QObject *obj = component.create(engine.rootContext());
    auto *window = qobject_cast<QQuickWindow *>(obj);
    if (!window) {
        delete obj;
        return;
    }

    const QRect geo = screen->geometry();
    window->setScreen(screen);
    window->setProperty("screenName", screen->name());
    window->setProperty("screenOriginX", geo.x());
    window->setProperty("screenOriginY", geo.y());
    // width/height still bound to Screen.* in QML after setScreen

    configureLayerShell(window);
    window->setMask(QRegion(0, 0, 1, 1));
    controller.addOverlayWindow(window, screen->name());
    controller.updateOverlayScreenGeometry(screen->name(), geo);
    window->show();

    QObject::connect(screen, &QScreen::geometryChanged, &controller,
                     [screen, window, &controller]() {
        const QRect g = screen->geometry();
        window->setProperty("screenOriginX", g.x());
        window->setProperty("screenOriginY", g.y());
        controller.updateOverlayScreenGeometry(screen->name(), g);
    });
};

for (QScreen *screen : QGuiApplication::screens())
    createOverlayForScreen(screen);

QObject::connect(&app, &QGuiApplication::screenAdded, &app, createOverlayForScreen);
QObject::connect(&app, &QGuiApplication::screenRemoved, &controller,
                 [&controller](QScreen *screen) {
    if (screen)
        controller.removeOverlayWindow(screen->name());
});
```

Notes:
* Keep global shortcut / KDBusService setup unchanged above this block.
* Do **not** call `engine.load(url)` for the overlay (avoids an extra orphan root). If tests or tooling expect `load`, document the change.
* Include `<QQmlComponent>`.

- [ ] **Step 3: Build and smoke on multi-monitor**

```bash
cmake --build build --target scribbleway-overlay
# Manual: start daemon on ≥2 outputs; confirm one layer surface per output (e.g. wayland-info / visual)
```

---

### Task 3: QML local ↔ virtual conversion

**Files:**
* Modify: `src/overlay/qml/main.qml`
* Touch shape delegates only if geometry write-back bypasses main.qml (BaseShape signals → currently update via controller from shape files — check bindings)

**Interfaces:**
* Consumes: `screenName`, `screenOriginX`, `screenOriginY` set from C++
* Produces: shapes stored in virtual coords; correct hit-testing per screen

- [ ] **Step 1: Declare per-window properties**

Near top of `Window` in `main.qml` (after `id: canvasWindow`):

```qml
    // Set from C++ before show (multi-monitor). Defaults keep single-screen/dev working.
    property string screenName: ""
    property real screenOriginX: 0
    property real screenOriginY: 0

    function toVirtual(px, py) {
        return Qt.point(px + screenOriginX, py + screenOriginY)
    }
    function toLocal(px, py) {
        return Qt.point(px - screenOriginX, py - screenOriginY)
    }
```

Keep `width: Screen.width` / `height: Screen.height` — after `setScreen`, `Screen` attached property is this window’s screen.

- [ ] **Step 2: Store virtual coords in finalizeShape**

In `finalizeShape()` (~113–178), convert every written coordinate:

```javascript
// freehand
shape["points"] = activePoints.map(p => {
    let v = toVirtual(p.x, p.y)
    return { x: v.x, y: v.y }  // match existing point map shape
})

// line/arrow
let startV = toVirtual(drawStartPoint.x, drawStartPoint.y)
shape["fromX"] = startV.x
shape["fromY"] = startV.y
shape["toX"] = startV.x + previewW   // preview deltas are local; equal in virtual
shape["toY"] = startV.y + previewH

// text / rect / ellipse
let originV = toVirtual(previewX, previewY) // or drawStartPoint for text
shape["x"] = originV.x
shape["y"] = originV.y
// width/height unchanged (axis-aligned sizes)
```

Live **previews** stay in local coords (they only draw on this window) — no change to previewX/Y/W/H logic.

- [ ] **Step 3: Render shapes with local offset**

In the Repeater delegate setup (`main.qml` ~214–238 and each `*Shape.qml` binding), subtract origin when reading model geometry.

Preferred pattern — pass adjusted props in each shape file’s root bindings. Example `RectangleShape.qml`:

```qml
shapeX: model.x - (canvasWindow ? canvasWindow.screenOriginX : 0)
```

But shape files do not see `canvasWindow` unless via `Window.window` attached property:

```qml
readonly property var host: Window.window
shapeX: model.x - (host && host.screenOriginX !== undefined ? host.screenOriginX : 0)
shapeY: model.y - (host && host.screenOriginY !== undefined ? host.screenOriginY : 0)
```

Apply the same for:
* `RectangleShape.qml`, `EllipseShape.qml`, `TextShape.qml` — `shapeX/Y/Width/Height`
* `LineShape.qml`, `ArrowShape.qml` — `shapeFromX/Y`, `shapeToX/Y`
* `FreehandShape.qml` — points mapping: each point minus origin when painting

**Write-back** (BaseShape `rectGeometryChanged` / `lineGeometryChanged` / freehand updates): add origin when calling `controller.updateShape`:

```javascript
// where shapes currently do controller.updateShape(index, { x: shapeX, y: shapeY, ... })
let host = Window.window
let ox = host && host.screenOriginX !== undefined ? host.screenOriginX : 0
let oy = host && host.screenOriginY !== undefined ? host.screenOriginY : 0
controller.updateShape(index, { x: shapeX + ox, y: shapeY + oy, width: shapeWidth, height: shapeHeight })
```

Grep all `updateShape` call sites under `src/overlay/qml/` and fix each.

Alternative (less file churn): wrap Repeater in:

```qml
Item {
    id: virtualLayer
    x: -canvasWindow.screenOriginX
    y: -canvasWindow.screenOriginY
    width: canvasWindow.width + Math.abs(canvasWindow.screenOriginX) // not strictly needed
    height: canvasWindow.height + Math.abs(canvasWindow.screenOriginY)
    // Repeater children anchors.fill virtualLayer; model coords = virtual
}
```

**Caveat:** `anchors.fill: parent` on BaseShape plus internal mouse areas can fight transforms; prefer explicit local binding subtraction (first approach) unless virtualLayer is verified with drag/resize.

- [ ] **Step 4: Selection rectangle → virtual**

In background `MouseArea` (~712–736):

```javascript
let tl = toVirtual(selectX, selectY)
controller.selectShapesInRect(tl.x, tl.y, selectW, selectH, mouse.modifiers & Qt.ShiftModifier)
```

`selectShapesInRect` in C++ already compares against model geometry — once model is virtual, passing virtual rects is enough (no C++ change).

- [ ] **Step 5: Input mask per screen**

In `requestInputRegionUpdate()` (~181–197):

```javascript
if (isPassthrough && !textEditor.visible) {
    controller.updateInputMaskForScreen(screenName, [])
} else if (isPassthrough && textEditor.visible) {
    controller.updateInputMaskForScreen(screenName, [
        Qt.rect(textEditor.x - 5, textEditor.y - 5, textEditor.width + 10, textEditor.height + 10)
    ])
} else {
    controller.updateInputMaskForScreen(screenName, [
        Qt.rect(0, 0, canvasWindow.width, canvasWindow.height)
    ])
}
```

Keyboard interactivity remains global via `controller.setKeyboardInteractivity(needsKeyboard)` (all windows). Only call `requestActivate()` on **this** window when it needs keys and pointer is here (optional improvement: activate window under cursor only).

- [ ] **Step 6: Paste shortcut passes virtual coords**

```qml
onActivated: {
    let v = toVirtual(canvasWindow.lastMousePos.x, canvasWindow.lastMousePos.y)
    controller.pasteFromClipboard(v.x, v.y)
}
```

- [ ] **Step 7: Text editor position**

`startTextEditing` positions `textEditor` from model x/y — convert with `toLocal` when placing the editor widget.

- [ ] **Step 8: Build + manual draw across monitors**

Draw a rectangle on screen A, confirm it appears only on A; drag toward edge — portion may show on B if geometry crosses (correct). Undo/selection still global.

---

### Task 4: Applet UI and backend cleanup

**Files:**
* Modify: `src/applet-plugin/appletbackend.h`
* Modify: `src/applet-plugin/appletbackend.cpp`
* Modify: `applet/contents/ui/FullRepresentation.qml`
* Modify: `applet/contents/ui/main.qml`
* Optional: `applet/contents/config/main.xml`

**Interfaces:**
* Consumes: multi-monitor always-on overlays
* Produces: no user-facing single-target switch

- [ ] **Step 1: Soft-deprecate targetScreen in AppletBackend**

Keep property for one release to avoid QML binding errors, but:

```cpp
void AppletBackend::setTargetScreen(const QString &screenName)
{
    if (m_targetScreen == screenName)
        return;
    m_targetScreen = screenName;
    Q_EMIT targetScreenChanged();
    // Do not call D-Bus setTargetScreen — overlays cover all screens.
}
```

Remove D-Bus sync of target screen in connection-setup (`appletbackend.cpp` ~258–261).

- [ ] **Step 2: Remove Target Screen row from FullRepresentation.qml**

Delete the `RowLayout` block at ~218–243 (“Target Screen:” + ComboBox). Optionally replace with a read-only label:

```qml
PlasmaComponents.Label {
    text: i18n("Screens: %1", root.backend.screenNames.join(", "))
    opacity: 0.7
}
```

Ensure `screenNames` updates on hotplug: today it is `CONSTANT` — change to:

```cpp
Q_PROPERTY(QStringList screenNames READ screenNames NOTIFY screenNamesChanged)
```

Connect `QGuiApplication::screenAdded/Removed` in `AppletBackend` ctor to emit `screenNamesChanged`.

- [ ] **Step 3: Stop startup setTargetScreen**

In `applet/contents/ui/main.qml` (~15–28), remove `Component.onCompleted` / Connections that call `backend.setTargetScreen`.

- [ ] **Step 4: Config entry**

Leave `targetScreen` in `main.xml` unused (harmless) or delete the entry. Prefer delete to avoid dead config.

- [ ] **Step 5: Build applet plugin**

```bash
cmake --build build --target scribblewaybackend
```

---

### Task 5: Tests

**Files:**
* Modify: `tests/shapesmodeltest.cpp`

- [ ] **Step 1: Controller multi-window unit tests**

Without real Wayland, use plain `QQuickWindow` instances (or `QWindow`) registered via `addOverlayWindow`:

```cpp
void ShapesModelTest::overlayMultiWindowRegistry()
{
    OverlayController controller;
    QQuickWindow w1;
    QQuickWindow w2;
    controller.addOverlayWindow(&w1, QStringLiteral("screen-a"));
    controller.addOverlayWindow(&w2, QStringLiteral("screen-b"));
    QCOMPARE(controller.overlayScreenNames().size(), 2);

    controller.updateInputMaskForScreen(QStringLiteral("screen-a"),
        QVariantList{ QRect(0, 0, 100, 100) });
    // mask applied only to w1 — compare w1.mask() if accessible

    controller.removeOverlayWindow(QStringLiteral("screen-a"));
    QCOMPARE(controller.overlayScreenNames(), QStringList{ QStringLiteral("screen-b") });
}
```

- [ ] **Step 2: setTargetScreen is no-op**

```cpp
controller.setTargetScreen(QStringLiteral("screen-b"));
// still two windows, neither hidden — assert surfaces count unchanged
```

- [ ] **Step 3: Virtual paste anchor**

Add shapes via paste with explicit virtual coords `(2000, 100)` and assert stored `x/y` match (no mapping through a single window).

- [ ] **Step 4: selectShapesInRect with virtual coords**

Add rect at virtual `(1920+10, 10)` (simulating second screen), select with virtual rect, assert selection.

- [ ] **Step 5: Keep AppletBackend targetScreen property test** but stop expecting D-Bus side effects (`tests/shapesmodeltest.cpp` ~1222–1224 remains valid for property set).

- [ ] **Step 6: Run tests**

```bash
cmake --build build --target shapesmodeltest && ctest --test-dir build -R shapesmodeltest --output-on-failure
```

---

## Test Plan (manual)

1. **Two monitors, same arrangement:** Start overlay; enter draw on each; shapes stay on the monitor where drawn; applet tool/color affects both.
2. **Span edge:** Draw a wide rectangle near the right edge of left monitor so it crosses; right monitor shows the overflow strip; select from either side.
3. **Hotplug:** Unplug secondary — its window goes away, shapes remain in model; replug — new window shows shapes that intersect that geometry.
4. **Passthrough:** Meta+Shift+X toggle; clicks reach apps on all monitors when passthrough; select mode captures all monitors.
5. **Keyboard:** Shortcuts (undo, tools) work after entering select/draw regardless of which monitor last hosted the pointer (all windows OnDemand).
6. **Paste:** Copy shape on A, move pointer to B, Ctrl+V — paste anchors near cursor on B (virtual).
7. **Excalidraw:** Copy from Scribbleway, paste into Excalidraw (and reverse) — still valid JSON; positions may be large if drawn on non-primary — acceptable.
8. **Old applet:** If `setTargetScreen` still called, daemon does not crash or hide overlays.

---

## Excalidraw Compatibility Impact

* **None on schema.** No new element fields.
* Scene coordinates become virtual-desktop absolute. Pasting Excalidraw content still uses element `x/y` as today (`convertFromExcalidraw`); placement offset via paste anchor remains.  
* Copying shapes drawn on a secondary monitor may export large `x` values (e.g. ≥ 1920). Excalidraw accepts that; users can reposition. No change required unless a future “normalize on export” is desired (out of scope).

---

## Acceptance Criteria

* [ ] With N connected screens, Scribbleway shows N layer-shell overlay surfaces (full output each), not one.
* [ ] Drawing, selecting, dragging, and text editing work on every screen without choosing a target.
* [ ] Shape data lives in one `ShapesModel`; undo/clear/selection are global.
* [ ] Coordinates are virtual-desktop; a shape near a monitor boundary can be visible on two windows.
* [ ] `setTargetScreen` no longer hide/show/moves a single window; multi-monitor works without the applet combo.
* [ ] Screen hotplug adds/removes overlay windows without crashing; `setCloseOnDismissed(false)`.
* [ ] Input masks and keyboard interactivity remain correct in passthrough vs select/draw.
* [ ] Existing unit tests pass; new registry/coord tests pass.
* [ ] Excalidraw copy/paste still round-trips shape types and style fields.

---

## Implementation Notes / Pitfalls

1. **Layer-shell is per-output** — never try one fullscreen window with virtual desktop size on Wayland.
2. **Empty `setMask`** — keep the 1×1 fallback per window (`overlaycontroller.cpp` comment ~358–360).
3. **Shortcut duplication** — each window instance has its own `Shortcut` objects; that is intentional so whichever surface has keyboard interactivity can fire them. Avoid double-firing by ensuring only interactive mode enables them (`shortcutGuard` already gates tool shortcuts).
4. **`QCursor::pos()`** — treat as virtual; do not `mapFromGlobal` through an arbitrary window after the multi-window change.
5. **Tests without compositor** — do not require real LayerShell in unit tests; registry + math only.
6. **Primary screen geometry (0,0)** — do not assume primary is at origin; always use `screen->geometry()`.
7. **QML `Screen` attached property** — tied to the window after `setScreen`; set screen **before** show.
8. **Scope string** — shared scope `"scribbleway-overlay"` is fine; unique scopes only if a compositor mis-stacks duplicates.

---

## Out of Scope

* Per-screen independent mode (draw on A, passthrough on B).
* Persist shapes across daemon restart (separate save/load plan).
* Re-basing virtual coordinates when the user rearranges monitor layout in KScreen.
* X11 multi-monitor (project is Wayland/LayerShell-first).
* Applet “move selection to screen” utilities.
