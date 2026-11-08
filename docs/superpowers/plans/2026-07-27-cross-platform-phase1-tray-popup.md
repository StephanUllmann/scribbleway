# Phase 1: Tray Popup + Single-Process Unification — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Retire the Plasma applet and replace it with a single-process `QSystemTrayIcon` + anchored popup UI ("Tray Popup"), used on the developer's own KDE/Wayland machine — the foundational step that unblocks Hyprland validation (Phase 2) and the macOS backend (Phase 3), per `docs/adr/0001-retire-plasma-applet-unify-single-process-ui.md`.

**Architecture:** `scribbleway-overlay` becomes the only binary. `OverlayController` gains the selection-state QML properties that used to live on the deleted `AppletBackend`, and is exposed to a second top-level QML window (the Tray Popup) via the same `controller` context property already used by the overlay window. Single-instance enforcement moves from `KDBusService` to a small `QLocalServer`-based guard. No virtual `OverlayPlatform` interface is introduced (see Q7 of the grilling session and the deleted `src/overlay/platform/` scaffold).

**Tech Stack:** C++20, Qt6 (Core, Gui, Widgets, Qml, Quick, DBus), KF6::GlobalAccel (Plasma-only hotkeys, unchanged), LayerShellQt (unchanged), CMake + ECM, QtTest.

## Global Constraints

- No X11, no GNOME/Wayland support — dropped entirely, not revisited in this plan (per ADR 0001 and grilling Q4/Q13).
- The Tray Popup must never render fullscreen — a prior attempt at this exact UI failed this way (recorded in ADR 0001).
- No `OverlayPlatform` virtual interface — plain code, no factory, no `unique_ptr<Base>` (Q7).
- Windows is out of scope for this plan entirely (no test hardware).
- Every task must leave `./dev-reinstall.sh` producing a working, installable build on the developer's live KDE/Wayland machine — this is a daily-driver machine, not disposable CI. Tasks that would leave the Plasma applet non-functional before its replacement is proven working are sequenced last, not first.
- Global Wayland hotkeys (Hyprland/`xdg-desktop-portal`) are explicitly out of scope for this plan (deferred per Q5).

---

## File Structure

**Delete (dead code, confirmed leftover from an abandoned attempt, safe to remove immediately):**
- `src/overlay/platform/` (`overlayplatform.h/.cpp`, `overlayplatform_{wayland,mac,x11,win}.cpp`) — unwired `OverlayPlatform` scaffold, contradicts the Q7 decision.
- `src/overlay/qml/Theme.qml` — unwired singleton; `FullRepresentation.qml`'s own inline theme object (below) already covers this without needing QML singleton registration.
- `install.sh`, `uninstall.sh`, `start-kde-manual.sh`, `Dockerfile.test`, `Dockerfile.vnc`, `entrypoint-vnc.sh` — superseded by `dev-reinstall.sh`.

**Delete (only once the Tray Popup is proven working — Task 6):**
- `src/applet-plugin/` (`appletbackend.h/.cpp`) — the D-Bus bridge to the (now-deleted) Plasma applet.
- `applet/` (`metadata.json`, `contents/ui/{main,FullRepresentation,CompactRepresentation}.qml`) — the Plasma applet package.

**Modify:**
- `src/overlay/overlaycontroller.h` / `.cpp` — add the selection-state and screen-targeting properties that used to live on `AppletBackend`.
- `src/overlay/main.cpp` — `QGuiApplication` → `QApplication`; `KDBusService`/D-Bus registration → `SingleInstanceGuard`; add `QSystemTrayIcon` + Tray Popup window wiring.
- `src/overlay/qml/FullRepresentation.qml` (the existing leftover port) — fix references to non-existent backend members, drop the dead "daemon not running" branch, add the new shape-type icons.
- `src/overlay/CMakeLists.txt` — new QML files, new icons, `Qt6::Widgets`, drop `KF6::DBusAddons`.
- `CMakeLists.txt` (root) — remove `BUILD_PLASMA_APPLET`, `Plasma`, `KF6::DBusAddons`, `add_subdirectory(src/applet-plugin)`, `plasma_install_package`.
- `tests/CMakeLists.txt` — drop `appletbackend.{h,cpp}` sources and include dir.
- `tests/shapesmodeltest.cpp` — delete `testAppletBackendIntegration`, add tests for the new `OverlayController` properties and the `SingleInstanceGuard`.
- `debian/control`, `debian/scribbleway.install` — drop the applet/Plasma/D-Bus-addons dependencies and install paths.
- `dev-reinstall.sh` — drop the Plasma-shell-restart step and stale plasmoid purge paths (only once the applet is actually gone — Task 6).

**Create:**
- `src/overlay/singleinstance.h` — small, testable single-instance guard (`QLocalServer`-based).
- `src/overlay/qml/TrayPopup.qml` — thin `Window` wrapper (non-fullscreen, `Qt.Popup` flag) hosting `FullRepresentation.qml`.
- `src/overlay/icons/draw-rectangle.svg`, `draw-ellipse.svg`, `draw-line.svg`, `draw-arrow.svg`, `draw-text.svg` — 5 icons matching the existing bundled set's style.
- `.github/workflows/ci.yml` — build + `ctest` on push (Linux only for now — no macOS backend exists until Phase 3).

---

### Task 1: Repo cleanup — delete dead scaffolding and unused dev scripts

**Files:**
- Delete: `src/overlay/platform/` (entire directory)
- Delete: `src/overlay/qml/Theme.qml`
- Delete: `install.sh`, `uninstall.sh`, `start-kde-manual.sh`, `Dockerfile.test`, `Dockerfile.vnc`, `entrypoint-vnc.sh`
- Delete (untracked, gitignored, local-only — plain `rm`, not `git rm`): `build/`, `build-deb/`, `build-flatpak/`, `build-flatpak-cache/`, `repo-flatpak/`, `.flatpak-builder/`, `debug.log`, `scribbleway_0.1-1_amd64.deb`, `scribbleway.flatpak`

**Interfaces:** None — this task touches no code that anything else depends on. `src/overlay/platform/` and `Theme.qml` are confirmed unreferenced by `src/overlay/CMakeLists.txt`'s `QML_FILES`/sources lists.

- [ ] **Step 1: Confirm nothing references the files about to be deleted**

```bash
grep -rn "platform/overlayplatform\|qml/Theme" src/overlay/CMakeLists.txt CMakeLists.txt src/overlay/main.cpp
```

Expected: no output (already verified during planning — this step is a safety net in case the working tree has changed).

- [ ] **Step 2: Delete the dead scaffolding and unused scripts**

```bash
git rm -r src/overlay/platform src/overlay/qml/Theme.qml
git rm install.sh uninstall.sh start-kde-manual.sh Dockerfile.test Dockerfile.vnc entrypoint-vnc.sh
rm -rf build build-deb build-flatpak build-flatpak-cache repo-flatpak .flatpak-builder debug.log scribbleway_0.1-1_amd64.deb scribbleway.flatpak
```

- [ ] **Step 3: Verify the build still succeeds**

```bash
./dev-reinstall.sh --no-plasma
```

Expected: builds and installs cleanly (the deleted files were never part of the build graph, so this should be a no-op change from the build's perspective). The Plasma applet is untouched and still works at this point.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "chore: delete unwired OverlayPlatform scaffold and unused dev scripts"
```

---

### Task 2: Expose selection-state and screen-targeting properties directly on OverlayController

The old `AppletBackend` mirrored `OverlayController::getSelectionState()` into individual QML properties (`selectedColor`, `selectedType`, ...) over D-Bus. The Tray Popup will bind to `OverlayController` directly (same process, no D-Bus), so those properties move onto `OverlayController` itself. `getSelectionState()` (returns a `QVariantMap` with keys `hasSelection`, `type`, `color`, `strokeWidth`, `opacity`, `fontFamily`, `fontSize`, `borderRadius`, `roughness`, `glow`, `fillColor`, `fillOpacity`, `freehandSmoothing`, `locked`, `selectedIndex`) already exists and is unchanged — these new properties just read from it.

**Files:**
- Modify: `src/overlay/overlaycontroller.h`
- Modify: `src/overlay/overlaycontroller.cpp`
- Test: `tests/shapesmodeltest.cpp`

**Interfaces:**
- Consumes: `OverlayController::getSelectionState()` (existing, unchanged), `OverlayController::selectionChanged(const QVariantMap&)` signal (existing, unchanged), `OverlayController::shapesMetadata()` (existing Q_SLOT, unchanged body).
- Produces: `hasSelection()`, `selectedType()`, `selectedColor()`, `selectedStrokeWidth()`, `selectedOpacity()`, `selectedFontFamily()`, `selectedFontSize()`, `selectedBorderRadius()`, `selectedRoughness()`, `selectedGlow()`, `selectedFillColor()`, `selectedFillOpacity()`, `selectedFreehandSmoothing()`, `selectedLocked()` — all `const`, all reading from `getSelectionState()`. `screenNames()` (`QStringList`), `targetScreen()` (`QString`) + `targetScreenChanged()` signal. `shapesMetadata` promoted from bare `Q_SLOT` to a real `Q_PROPERTY` (the method itself is unchanged — `shapesMetadataChanged` is already emitted everywhere shapes change, per `notifyShapesChanged()`).

- [ ] **Step 1: Write the failing test**

Add to `tests/shapesmodeltest.cpp`'s private slots declaration (near the other `testOverlayController*` tests, e.g. after `testOverlayControllerProperties`):

```cpp
    void testOverlayControllerSelectionProperties();
```

Add the test body (anywhere among the other `OverlayController` tests):

```cpp
void ShapesModelTest::testOverlayControllerSelectionProperties()
{
    OverlayController controller;

    // No selection: falls back to defaults, matching getSelectionState()
    QCOMPARE(controller.hasSelection(), false);
    QCOMPARE(controller.selectedColor(), controller.defaultColor());
    QCOMPARE(controller.selectedStrokeWidth(), controller.defaultStrokeWidth());

    QVariantMap shape;
    shape[QStringLiteral("type")] = QStringLiteral("rectangle");
    shape[QStringLiteral("color")] = QStringLiteral("#00ffff");
    shape[QStringLiteral("strokeWidth")] = 7;
    controller.addShape(shape);

    QCOMPARE(controller.hasSelection(), true);
    QCOMPARE(controller.selectedType(), QStringLiteral("rectangle"));
    QCOMPARE(controller.selectedColor(), QStringLiteral("#00ffff"));
    QCOMPARE(controller.selectedStrokeWidth(), 7);

    QVERIFY(!controller.screenNames().isEmpty());
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target shapesmodeltest -j"$(nproc)" 2>&1 | tail -30
```

Expected: FAIL to compile — `hasSelection`, `selectedType`, `selectedColor`, `selectedStrokeWidth`, `screenNames` are not members of `OverlayController`.

- [ ] **Step 3: Add the properties to `overlaycontroller.h`**

Add to the `Q_PROPERTY` block (after the existing `localShortcutSequences` property, line 57):

```cpp
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedType READ selectedType NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedColor READ selectedColor NOTIFY selectionChanged)
    Q_PROPERTY(int selectedStrokeWidth READ selectedStrokeWidth NOTIFY selectionChanged)
    Q_PROPERTY(double selectedOpacity READ selectedOpacity NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedFontFamily READ selectedFontFamily NOTIFY selectionChanged)
    Q_PROPERTY(int selectedFontSize READ selectedFontSize NOTIFY selectionChanged)
    Q_PROPERTY(int selectedBorderRadius READ selectedBorderRadius NOTIFY selectionChanged)
    Q_PROPERTY(int selectedRoughness READ selectedRoughness NOTIFY selectionChanged)
    Q_PROPERTY(int selectedGlow READ selectedGlow NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedFillColor READ selectedFillColor NOTIFY selectionChanged)
    Q_PROPERTY(double selectedFillOpacity READ selectedFillOpacity NOTIFY selectionChanged)
    Q_PROPERTY(int selectedFreehandSmoothing READ selectedFreehandSmoothing NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedLocked READ selectedLocked NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList shapesMetadata READ shapesMetadata NOTIFY shapesMetadataChanged)
    Q_PROPERTY(QStringList screenNames READ screenNames CONSTANT)
    Q_PROPERTY(QString targetScreen READ targetScreen NOTIFY targetScreenChanged)
```

Add the matching declarations to the `public:` section (after `bool hasMultiSelection() const;`):

```cpp
    bool hasSelection() const;
    QString selectedType() const;
    QString selectedColor() const;
    int selectedStrokeWidth() const;
    double selectedOpacity() const;
    QString selectedFontFamily() const;
    int selectedFontSize() const;
    int selectedBorderRadius() const;
    int selectedRoughness() const;
    int selectedGlow() const;
    QString selectedFillColor() const;
    double selectedFillOpacity() const;
    int selectedFreehandSmoothing() const;
    bool selectedLocked() const;
    QStringList screenNames() const;
    QString targetScreen() const;
```

Add the new signal (in the `Q_SIGNALS:` block, near `localShortcutsChanged()`):

```cpp
    void targetScreenChanged();
```

Add the new private member (near `QString m_activeTool;`):

```cpp
    QString m_targetScreen;
```

- [ ] **Step 4: Implement in `overlaycontroller.cpp`**

Add near `getSelectionState()`:

```cpp
bool OverlayController::hasSelection() const
{
    return m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount();
}

QString OverlayController::selectedType() const
{
    return getSelectionState().value(QStringLiteral("type")).toString();
}

QString OverlayController::selectedColor() const
{
    return getSelectionState().value(QStringLiteral("color")).toString();
}

int OverlayController::selectedStrokeWidth() const
{
    return getSelectionState().value(QStringLiteral("strokeWidth")).toInt();
}

double OverlayController::selectedOpacity() const
{
    return getSelectionState().value(QStringLiteral("opacity")).toDouble();
}

QString OverlayController::selectedFontFamily() const
{
    return getSelectionState().value(QStringLiteral("fontFamily")).toString();
}

int OverlayController::selectedFontSize() const
{
    return getSelectionState().value(QStringLiteral("fontSize")).toInt();
}

int OverlayController::selectedBorderRadius() const
{
    return getSelectionState().value(QStringLiteral("borderRadius")).toInt();
}

int OverlayController::selectedRoughness() const
{
    return getSelectionState().value(QStringLiteral("roughness")).toInt();
}

int OverlayController::selectedGlow() const
{
    return getSelectionState().value(QStringLiteral("glow")).toInt();
}

QString OverlayController::selectedFillColor() const
{
    return getSelectionState().value(QStringLiteral("fillColor")).toString();
}

double OverlayController::selectedFillOpacity() const
{
    return getSelectionState().value(QStringLiteral("fillOpacity")).toDouble();
}

int OverlayController::selectedFreehandSmoothing() const
{
    return getSelectionState().value(QStringLiteral("freehandSmoothing")).toInt();
}

bool OverlayController::selectedLocked() const
{
    return getSelectionState().value(QStringLiteral("locked")).toBool();
}

QStringList OverlayController::screenNames() const
{
    QStringList names;
    for (QScreen *screen : QGuiApplication::screens()) {
        names << screen->name();
    }
    return names;
}

QString OverlayController::targetScreen() const
{
    return m_targetScreen;
}
```

Update `setTargetScreen()` to track and emit (find the existing method around line 780 — add the tracking after the successful move, before the closing brace):

```cpp
void OverlayController::setTargetScreen(const QString &screenName)
{
    if (!m_window || screenName.isEmpty()) return;

    QScreen *target = nullptr;
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->name() == screenName) {
            target = screen;
            break;
        }
    }

    if (target && m_window->screen() != target) {
        m_window->hide();
        m_window->setScreen(target);
        // ... existing show-again logic stays unchanged below this point
    }

    if (m_targetScreen != screenName) {
        m_targetScreen = screenName;
        Q_EMIT targetScreenChanged();
    }
}
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build build --target shapesmodeltest -j"$(nproc)" && ./build/bin/shapesmodeltest 2>&1 | tail -20
```

Expected: PASS, including the new `testOverlayControllerSelectionProperties`.

- [ ] **Step 6: Run the full test suite to check for regressions**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all tests pass, including the still-present `testAppletBackendIntegration` (untouched in this task).

- [ ] **Step 7: Commit**

```bash
git add src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp tests/shapesmodeltest.cpp
git commit -m "feat(controller): expose selection-state and screen properties directly on OverlayController"
```

---

### Task 3: Single-instance guard (QLocalServer-based, not yet wired into main.cpp)

Built and tested standalone first — `main.cpp` starts using it in Task 6, alongside the KDBusService removal, to avoid regressing the still-active Plasma applet's D-Bus-based `overlayConnected` detection before its replacement exists.

**Files:**
- Create: `src/overlay/singleinstance.h`
- Test: `tests/shapesmodeltest.cpp`

**Interfaces:**
- Produces: `class SingleInstanceGuard { explicit SingleInstanceGuard(const QString &name); bool tryAcquire(); }` — `tryAcquire()` returns `true` and holds the lock for the guard's lifetime if no other instance holds it, `false` otherwise.

- [ ] **Step 1: Write the failing test**

Add to the test declarations:

```cpp
    void testSingleInstanceGuard();
```

Add the test body:

```cpp
void ShapesModelTest::testSingleInstanceGuard()
{
    SingleInstanceGuard first(QStringLiteral("scribbleway-test-lock"));
    QVERIFY(first.tryAcquire());

    SingleInstanceGuard second(QStringLiteral("scribbleway-test-lock"));
    QVERIFY(!second.tryAcquire());
}
```

Add `#include "singleinstance.h"` to the top of `tests/shapesmodeltest.cpp`.

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target shapesmodeltest 2>&1 | tail -20
```

Expected: FAIL to compile — `singleinstance.h` doesn't exist yet.

- [ ] **Step 3: Implement `src/overlay/singleinstance.h`**

```cpp
#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

class SingleInstanceGuard
{
public:
    explicit SingleInstanceGuard(const QString &name) : m_name(name) {}

    bool tryAcquire()
    {
        QLocalSocket probe;
        probe.connectToServer(m_name);
        if (probe.waitForConnected(100)) {
            probe.disconnectFromServer();
            return false;
        }

        QLocalServer::removeServer(m_name);
        return m_server.listen(m_name);
    }

private:
    QString m_name;
    QLocalServer m_server;
};
```

- [ ] **Step 4: Wire the new header into the test build**

Modify `tests/CMakeLists.txt` — add `../src/overlay/singleinstance.h` to the `add_executable(shapesmodeltest ...)` sources list (it's header-only, but listing it keeps it visible in IDEs; the `#include` alone is sufficient for compilation since it's already under `target_include_directories`).

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build build --target shapesmodeltest && ./build/bin/shapesmodeltest 2>&1 | tail -20
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/overlay/singleinstance.h tests/shapesmodeltest.cpp tests/CMakeLists.txt
git commit -m "feat: add QLocalServer-based single-instance guard (not yet wired into main)"
```

---

### Task 4: Tray Popup QML — fix the leftover FullRepresentation.qml port, add missing icons

`src/overlay/qml/FullRepresentation.qml` already exists (from the same leftover commit as the deleted `platform/` scaffold) and is a reasonable start: Qt Quick Controls only, no Kirigami, `qrc:/icons/*.svg` icon wiring already matches the bundled icon set. It has three problems: it calls `backend.clearAll()` and `backend.startOverlay()`, neither of which exist on `OverlayController`; it has a dead "daemon not running" branch that no longer applies now that the popup lives in the same process as the overlay; and its dual-mode `root.backend`-or-`controller` lookup is dead complexity now that there's no Plasma applet ever again.

**Files:**
- Modify: `src/overlay/qml/FullRepresentation.qml`
- Create: `src/overlay/icons/draw-rectangle.svg`, `draw-ellipse.svg`, `draw-line.svg`, `draw-arrow.svg`, `draw-text.svg`

**Interfaces:**
- Consumes: `OverlayController` properties from Task 2 (`hasSelection`, `selectedType`, `selectedColor`, `selectedStrokeWidth`, `selectedOpacity`, `selectedFillColor`, `selectedFillOpacity`, `selectedGlow`, `selectedFreehandSmoothing`, `selectedRoughness`, `selectedBorderRadius`, `selectedFontFamily`, `selectedFontSize`, `shapesMetadata`, `screenNames`, `targetScreen`), plus existing `OverlayController` members (`activeTool`, `defaultColor`, `defaultStrokeWidth`, `defaultOpacity`, `undo()`, `redo()`, `clear()`, `deleteSelected()`, `raiseSelected()`, `lowerSelected()`, `setActiveTool()`, `setTargetScreen()`, `setColor()` — wait, `setColor` doesn't exist on `OverlayController`; see Step 1 below), `setFillColor()`, `setFillOpacity()`, `setBorderRadius()`, `setGlow()`, `setFreehandSmoothing()`, `setRoughness()`, `setFontFamily()`, `setFontSize()`, `setShapeLocked()`, `deleteShape()`, `selectShape()`.
- Produces: a QML `Item` root (`FullRepresentation`) sized for a fixed popup, consumed by `TrayPopup.qml` in Task 5.

- [ ] **Step 1: Check the `set*` slot names this file assumes actually exist on `OverlayController`**

```bash
grep -n "Q_INVOKABLE\|public Q_SLOTS" -A1 src/overlay/overlaycontroller.h | grep -i "setColor\|setStrokeWidth\|setOpacity\|setFillColor\|setFillOpacity\|setGlow\|setFreehandSmoothing\|setRoughness\|setBorderRadius\|setFontFamily\|setFontSize"
```

`OverlayController` doesn't have a bare `setColor(QString)` — property setters exist only for the `default*` properties (`setDefaultColor`, `setDefaultStrokeWidth`, etc. — used when there's no selection) and `updateProperties(const QVariantMap&)` (used when there is a selection, per the existing `updateProperties` Q_SLOT). The old `AppletBackend::setColor()` internally chose between these two based on `hasSelection`. Add this same dispatch as a small helper directly in the QML (it doesn't need a C++ method — it's pure UI-layer routing over calls that already exist):

Add near the top of `FullRepresentation.qml`, after the `backend` property:

```qml
    function setColor(color) {
        if (backend.hasSelection) backend.updateProperties({color: color})
        else backend.setDefaultColor(color)
    }
    function setStrokeWidth(width) {
        if (backend.hasSelection) backend.updateProperties({strokeWidth: width})
        else backend.setDefaultStrokeWidth(width)
    }
    function setOpacity(opacity) {
        if (backend.hasSelection) backend.updateProperties({opacity: opacity})
        else backend.setDefaultOpacity(opacity)
    }
    function setFillColor(color) {
        if (backend.hasSelection) backend.updateProperties({fillColor: color})
        else backend.setDefaultFillColor(color)
    }
    function setFillOpacity(opacity) {
        if (backend.hasSelection) backend.updateProperties({fillOpacity: opacity})
        else backend.setDefaultFillOpacity(opacity)
    }
    function setGlow(glow) {
        if (backend.hasSelection) backend.updateProperties({glow: glow})
        else backend.setDefaultGlow(glow)
    }
    function setFreehandSmoothing(level) {
        if (backend.hasSelection) backend.updateProperties({freehandSmoothing: level})
        else backend.setDefaultFreehandSmoothing(level)
    }
    function setRoughness(roughness) {
        if (backend.hasSelection) backend.updateProperties({roughness: roughness})
        else backend.setDefaultRoughness(roughness)
    }
    function setBorderRadius(radius) {
        if (backend.hasSelection) backend.updateProperties({borderRadius: radius})
        else backend.setDefaultBorderRadius(radius)
    }
    function setFontFamily(family) {
        if (backend.hasSelection) backend.updateProperties({fontFamily: family})
        else backend.setDefaultFontFamily(family)
    }
    function setFontSize(size) {
        if (backend.hasSelection) backend.updateProperties({fontSize: size})
        else backend.setDefaultFontSize(size)
    }
```

Then replace every `backend.setColor(`, `backend.setStrokeWidth(`, `backend.setOpacity(`, `backend.setFillColor(`, `backend.setFillOpacity(`, `backend.setGlow(`, `backend.setFreehandSmoothing(`, `backend.setRoughness(`, `backend.setBorderRadius(`, `backend.setFontFamily(`, `backend.setFontSize(` call site in the file with the corresponding local `setColor(`, `setStrokeWidth(`, etc. (drop the `backend.` prefix — these are now local functions).

- [ ] **Step 2: Simplify the `backend` property (no more dual-mode)**

Replace:

```qml
    // Dynamic backend provider for both Plasma applet (root.backend) and Standalone mode (controller)
    readonly property var backend: (typeof root !== "undefined" && root && root.backend) ? root.backend : (typeof controller !== "undefined" ? controller : null)
```

with:

```qml
    readonly property var backend: controller
```

(`controller` is the root-context property already set in `main.cpp`; it will be visible here once `TrayPopup.qml` is loaded by the same `QQmlApplicationEngine` in Task 5.)

Since `backend` can no longer be `null`, remove every `backend &&`/`backend ?` null-guard in the file that exists purely for the old dual-mode case — e.g. `(backend && backend.hasSelection)` becomes `backend.hasSelection`, `backend ? backend.defaultColor : "#e63946"` becomes `backend.defaultColor`. Do this pass across the whole file; there is no behavioral change, just removing dead defensiveness for a case (`backend === null`) that can no longer happen.

- [ ] **Step 3: Delete the dead "not running" branch and status label**

Delete the entire "Not Running View" `ColumnLayout` block (the one with `text: "Scribbleway daemon is not running."` and the "Start Scribbleway" button).

Change the "Running View" `ColumnLayout`'s `visible: !backend || backend.overlayConnected` to just remove the `visible:` line entirely (always visible now).

In the header `RowLayout`, delete the `Controls.Label { text: (backend && backend.overlayConnected) ? "Running" : "Stopped" ... }` line — keep just the "Scribbleway" title `Text`.

- [ ] **Step 4: Fix `clearAll()` → `clear()`**

Replace `backend.clearAll()` with `backend.clear()` (the one call site, in the "Clear All" button's `onClicked`).

- [ ] **Step 5: Wire the real shape-type icons**

Replace the shape-list icon `switch` statement:

```qml
                                    source: {
                                        let t = modelData && modelData.type ? modelData.type.toLowerCase() : "";
                                        switch(t) {
                                            case "rectangle": return "qrc:/icons/edit-select.svg";
                                            case "ellipse": return "qrc:/icons/edit-select.svg";
                                            case "arrow": return "qrc:/icons/arrow-right.svg";
                                            case "line": return "qrc:/icons/arrow-right.svg";
                                            case "text": return "qrc:/icons/draw-freehand.svg";
                                            default: return "qrc:/icons/draw-freehand.svg";
                                        }
                                    }
```

with:

```qml
                                    source: {
                                        let t = modelData && modelData.type ? modelData.type.toLowerCase() : "";
                                        switch(t) {
                                            case "rectangle": return "qrc:/icons/draw-rectangle.svg";
                                            case "ellipse": return "qrc:/icons/draw-ellipse.svg";
                                            case "arrow": return "qrc:/icons/draw-arrow.svg";
                                            case "line": return "qrc:/icons/draw-line.svg";
                                            case "text": return "qrc:/icons/draw-text.svg";
                                            default: return "qrc:/icons/draw-freehand.svg";
                                        }
                                    }
```

- [ ] **Step 6: Create the 5 missing icons**

`src/overlay/icons/draw-rectangle.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="5" width="18" height="14" rx="1"/>
</svg>
```

`src/overlay/icons/draw-ellipse.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <ellipse cx="12" cy="12" rx="9" ry="6"/>
</svg>
```

`src/overlay/icons/draw-line.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <line x1="4" y1="20" x2="20" y2="4"/>
</svg>
```

`src/overlay/icons/draw-arrow.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <line x1="4" y1="20" x2="20" y2="4"/>
  <path d="M20 4h-7M20 4v7"/>
</svg>
```

`src/overlay/icons/draw-text.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M4 5h16M12 5v14M9 19h6"/>
</svg>
```

- [ ] **Step 7: Register the new files in the build**

Modify `src/overlay/CMakeLists.txt` — add to the `QML_FILES` list:

```cmake
        qml/FullRepresentation.qml
```

Add a `RESOURCES` block. Confirmed during planning: `icons/` is **not** wired into the build anywhere today — there's no `.qrc` file, no existing `qt_add_resources` call, and none of the currently-built QML files (`main.qml`, `ToolCursorBadge.qml`, `shapes/*.qml`) reference `qrc:/icons` at all. The 14 existing SVGs under `src/overlay/icons/` are, like `platform/` and `Theme.qml`, unused leftovers from the same abandoned commit — this is the first time any of them actually get compiled in. Add:

```cmake
qt_add_resources(scribbleway-overlay "icons"
    PREFIX "/icons"
    BASE icons
    FILES
        icons/arrow-down.svg
        icons/arrow-right.svg
        icons/color-picker.svg
        icons/draw-freehand.svg
        icons/draw-rectangle.svg
        icons/draw-ellipse.svg
        icons/draw-line.svg
        icons/draw-arrow.svg
        icons/draw-text.svg
        icons/edit-clear.svg
        icons/edit-delete.svg
        icons/edit-redo.svg
        icons/edit-select.svg
        icons/edit-undo.svg
        icons/go-down.svg
        icons/go-up.svg
        icons/object-locked.svg
        icons/object-unlocked.svg
        icons/run-build.svg
)
```

- [ ] **Step 8: Build and visually sanity-check the QML in isolation**

```bash
cmake --build build --target scribbleway-overlay -j"$(nproc)" 2>&1 | tail -40
```

Expected: builds clean, no QML binding errors printed (Qt prints `qrc:/scribbleway/qml/FullRepresentation.qml:N: ReferenceError: ...` to stderr for any remaining bad references — there should be none).

- [ ] **Step 9: Commit**

```bash
git add src/overlay/qml/FullRepresentation.qml src/overlay/icons/*.svg src/overlay/CMakeLists.txt
git commit -m "feat(qml): fix leftover FullRepresentation port, add shape-type icons"
```

---

### Task 5: Wire QSystemTrayIcon + Tray Popup window into main.cpp (Plasma applet still present, unaffected)

**Files:**
- Create: `src/overlay/qml/TrayPopup.qml`
- Modify: `src/overlay/main.cpp`
- Modify: `src/overlay/CMakeLists.txt`

**Interfaces:**
- Consumes: `FullRepresentation.qml` (Task 4), `controller` root-context property (existing, set in `main.cpp`).
- Produces: nothing consumed by later tasks except the working popup itself — Task 6 removes the parts of `main.cpp` this task doesn't touch (D-Bus registration).

- [ ] **Step 1: Create `src/overlay/qml/TrayPopup.qml`**

```qml
import QtQuick
import QtQuick.Window

Window {
    id: trayPopup
    width: 432
    height: 550
    minimumWidth: 400
    minimumHeight: 520
    flags: Qt.Popup | Qt.FramelessWindowHint
    visible: false
    color: sysPalette.window

    SystemPalette {
        id: sysPalette
    }

    FullRepresentation {
        anchors.fill: parent
        anchors.margins: 8
    }
}
```

- [ ] **Step 2: Register it in `src/overlay/CMakeLists.txt`**

Add to `QML_FILES` (alongside `qml/FullRepresentation.qml` from Task 4):

```cmake
        qml/TrayPopup.qml
```

- [ ] **Step 3: Switch `main.cpp` from `QGuiApplication` to `QApplication`**

`QSystemTrayIcon` lives in `Qt6::Widgets`; `QApplication` is a drop-in superset of `QGuiApplication` and is compatible with a QML-only UI (no `QWidget`s are created).

Replace the include:

```cpp
#include <QGuiApplication>
```

with:

```cpp
#include <QApplication>
```

Replace:

```cpp
    QGuiApplication app(argc, argv);
```

with:

```cpp
    QApplication app(argc, argv);
```

Add includes:

```cpp
#include <QSystemTrayIcon>
#include <QIcon>
#include <QScreen>
```

(`QScreen` is likely already included; don't duplicate if so.)

- [ ] **Step 4: Load `TrayPopup.qml` as a second top-level window and wire the tray icon**

Add after the existing `engine.load(url);` line, before `return app.exec();`:

```cpp
    const QUrl trayPopupUrl(QStringLiteral("qrc:/scribbleway/qml/TrayPopup.qml"));
    QQuickWindow *popupWindow = nullptr;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [trayPopupUrl, &popupWindow](QObject *obj, const QUrl &objUrl) {
        if (objUrl == trayPopupUrl) {
            popupWindow = qobject_cast<QQuickWindow*>(obj);
        }
    }, Qt::QueuedConnection);
    engine.load(trayPopupUrl);

    QSystemTrayIcon trayIcon(QIcon(QStringLiteral(":/icons/draw-freehand.svg")));
    trayIcon.setToolTip(QStringLiteral("Scribbleway"));
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, &app,
                     [&popupWindow](QSystemTrayIcon::ActivationReason reason) {
        if (reason != QSystemTrayIcon::Trigger) return;
        if (!popupWindow) return;
        if (popupWindow->isVisible()) {
            popupWindow->hide();
            return;
        }
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            const QRect avail = screen->availableGeometry();
            popupWindow->setPosition(avail.right() - popupWindow->width() - 8, avail.bottom() - popupWindow->height() - 8);
        }
        popupWindow->show();
        popupWindow->raise();
        popupWindow->requestActivate();
    });
    trayIcon.show();
```

`popupWindow` is captured by reference in both lambdas rather than boxed in a `shared_ptr` — it's a local in `main()` that stays alive for the entire `app.exec()` call, which both lambdas run within, so a plain reference is sufficient. The pointer starts `nullptr` and is only set once the engine finishes asynchronously loading `TrayPopup.qml` (`objectCreated` fires before `trayIcon.show()` returns control to the event loop in practice, but the `if (!popupWindow) return;` guard in the `activated` handler covers the case where a click somehow arrives before that).

- [ ] **Step 5: Build**

```bash
cmake --build build --target scribbleway-overlay -j"$(nproc)" 2>&1 | tail -40
```

Expected: builds clean. If it fails linking with undefined `QSystemTrayIcon` symbols, `Qt6::Widgets` isn't linked yet — see Step 6.

- [ ] **Step 6: Link Qt6::Widgets**

Modify `src/overlay/CMakeLists.txt` — add `Widgets` to the top-level `find_package(Qt6 ... COMPONENTS ...)` list in the root `CMakeLists.txt`:

```cmake
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Qml
    Quick
    DBus
)
```

And add `Qt6::Widgets` to `target_link_libraries(scribbleway-overlay PRIVATE ...)` in `src/overlay/CMakeLists.txt`.

- [ ] **Step 7: Rebuild and manually verify on the live KDE/Wayland machine**

```bash
./dev-reinstall.sh --no-plasma
```

Manual verification checklist (this interaction — `QSystemTrayIcon` + a `Qt.Popup`-flagged window — has known cross-platform quirks around focus-loss dismissal racing with the triggering click; verify by hand rather than trusting the code on paper):

- [ ] A new tray icon appears (alongside the still-present Plasma applet icon).
- [ ] Clicking it shows the popup near the bottom-right of the screen, at a fixed size — **never fullscreen**.
- [ ] Clicking the tray icon again closes the popup (not reopens it immediately).
- [ ] Clicking outside the popup closes it.
- [ ] The popup's controls (undo/redo/color/tool selection) actually affect the overlay.

If the second-click-closes behavior misbehaves (a real risk: `Qt.Popup`'s focus-out auto-hide can race with the tray icon's own click event), fix by tracking `wasVisibleBeforeThisClick` explicitly in the lambda rather than checking `win->isVisible()` at click time — note this as a likely follow-up if Step 7's manual check fails.

- [ ] **Step 8: Commit**

```bash
git add src/overlay/qml/TrayPopup.qml src/overlay/main.cpp src/overlay/CMakeLists.txt CMakeLists.txt
git commit -m "feat: add QSystemTrayIcon + Tray Popup window, running alongside the Plasma applet"
```

---

### Task 6: Cutover — remove the Plasma applet, KDBusService, and BUILD_PLASMA_APPLET

Only start this task once Task 5's manual verification checklist passes — this is the task that removes the fallback (the Plasma applet) and the mechanism it depended on (D-Bus service registration).

**Files:**
- Delete: `src/applet-plugin/` (entire directory)
- Delete: `applet/` (entire directory)
- Modify: `src/overlay/main.cpp`
- Modify: `src/overlay/overlaycontroller.h`
- Modify: `src/overlay/CMakeLists.txt`
- Modify: `CMakeLists.txt` (root)
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/shapesmodeltest.cpp`
- Modify: `debian/control`, `debian/scribbleway.install`
- Modify: `dev-reinstall.sh`

**Interfaces:**
- Consumes: `SingleInstanceGuard` (Task 3).
- Produces: nothing — this is the last task in Phase 1.

- [ ] **Step 1: Replace KDBusService with SingleInstanceGuard in `main.cpp`**

Remove:

```cpp
#include <KGlobalAccel>
#include <KDBusService>
#include <QDBusConnection>
```

replace with:

```cpp
#include <KGlobalAccel>
#include "singleinstance.h"
```

(`KGlobalAccel` stays — hotkeys under Plasma are unchanged.)

Remove:

```cpp
    // Ensure unique application instance and register org.kde.scribbleway service
    KDBusService dbusService(KDBusService::Unique);

    OverlayController controller;

    // Register QObject on D-Bus Session Bus
    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/Overlay"),
        &controller,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
    );
```

replace with:

```cpp
    SingleInstanceGuard instanceGuard(QStringLiteral("scribbleway"));
    if (!instanceGuard.tryAcquire()) {
        qWarning() << "Scribbleway is already running.";
        return 0;
    }

    OverlayController controller;
```

- [ ] **Step 2: Remove the now-dead D-Bus class info from `overlaycontroller.h`**

Remove:

```cpp
    Q_CLASSINFO("D-Bus Interface", "org.kde.scribbleway.OverlayController")
```

(`Qt6::DBus` itself stays linked — `src/common/dbusutils.h`'s `DBusUtils::demarshal()` still uses `QDBusArgument`/`QDBusVariant` types and is still called from `addShape`/`updateProperties`/`updateShape`. Only the *registration* on the session bus goes away.)

- [ ] **Step 3: Delete the Plasma applet**

```bash
git rm -r src/applet-plugin applet
```

- [ ] **Step 4: Update `src/overlay/CMakeLists.txt`**

Remove `KF6::DBusAddons` from `target_link_libraries(scribbleway-overlay PRIVATE ...)`.

- [ ] **Step 5: Update the root `CMakeLists.txt`**

Remove the `BUILD_PLASMA_APPLET` option, the `Plasma` `find_package`, `add_subdirectory(src/applet-plugin)`, and `plasma_install_package`. Remove `DBusAddons` from the `KF6` `find_package` components. Simplify the `BUILD_DEB_PACKAGE` custom target's dependency list to just `scribbleway-overlay` unconditionally.

Result:

```cmake
cmake_minimum_required(VERSION 3.20)
project(scribbleway VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KF_MIN_VERSION "6.0.0")
set(QT_MIN_VERSION "6.6.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-missing-include-dirs)
endif()

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Qml
    Quick
    DBus
)

include(ECMQmlModule)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    GlobalAccel
)

find_package(LayerShellQt REQUIRED)

add_subdirectory(src/overlay)
enable_testing()
add_subdirectory(tests)

install(FILES org.kde.scribbleway.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.scribbleway-autostart.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})

option(BUILD_DEB_PACKAGE "Automatically rebuild the .deb package after a successful build" ON)

if(BUILD_DEB_PACKAGE)
    if(NOT DEFINED ENV{DEB_BUILD_ARCH} AND NOT DEFINED ENV{DPKG_RUNNING_VERSION})
        add_custom_target(rebuild_debian_package ALL
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/packaging/build_deb.sh ${CMAKE_BINARY_DIR}
            DEPENDS scribbleway-overlay
            COMMENT "Building Debian package (.deb) from build outputs"
            VERBATIM
        )
    endif()
endif()
```

- [ ] **Step 6: Update `tests/CMakeLists.txt`**

Remove `../src/applet-plugin/appletbackend.h` and `../src/applet-plugin/appletbackend.cpp` from the `add_executable(shapesmodeltest ...)` sources. Remove `${CMAKE_CURRENT_SOURCE_DIR}/../src/applet-plugin` from `target_include_directories`.

- [ ] **Step 7: Delete the obsolete D-Bus integration test**

In `tests/shapesmodeltest.cpp`:
- Remove the declaration `void testAppletBackendIntegration();` from the private slots list.
- Delete the entire `void ShapesModelTest::testAppletBackendIntegration() { ... }` function body (spans from its `void ShapesModelTest::testAppletBackendIntegration()` line to the closing brace immediately before the next test function, `testReworkedShortcutsSlots`).
- Remove the now-unused includes: `#include <QDBusConnection>`, `#include <QDBusInterface>`, `#include <QDBusReply>`, `#include "appletbackend.h"`.

- [ ] **Step 8: Update `debian/control`**

Remove `libkf6dbusaddons-dev,` and `libplasma-dev | libplasma6-dev,` from `Build-Depends`.

- [ ] **Step 9: Update `debian/scribbleway.install`**

Remove:

```
usr/share/plasma/plasmoids/org.kde.scribbleway/*
usr/lib/*/qt6/qml/org/kde/scribbleway/backend/*
```

Result:

```
usr/bin/scribbleway-overlay
usr/share/applications/org.kde.scribbleway.desktop
etc/xdg/autostart/org.kde.scribbleway-autostart.desktop
```

- [ ] **Step 10: Simplify `dev-reinstall.sh`**

Remove the `--no-plasma` option and `RESTART_PLASMA` variable/logic entirely (restarting Plasma shell was only ever needed to reload the plasmoid definition — there is no plasmoid anymore). Remove the now-permanently-dead purge targets from `TARGET_DIRS`:

```
"$HOME/.local/share/plasma/plasmoids/org.kde.scribbleway"
"/usr/local/share/plasma/plasmoids/org.kde.scribbleway"
"/usr/share/plasma/plasmoids/org.kde.scribbleway"
"$HOME/.local/lib/qml/org/kde/scribbleway"
"$HOME/.local/lib/x86_64-linux-gnu/qml/org/kde/scribbleway"
"/usr/lib/qt6/qml/org/kde/scribbleway"
"/usr/lib64/qt6/qml/org/kde/scribbleway"
"/usr/local/lib/qt6/qml/org/kde/scribbleway"
"/usr/local/lib/x86_64-linux-gnu/qt6/qml/org/kde/scribbleway"
```

(Keep the binary/desktop-file purge targets and the `"=== 6. Process lifecycle management ==="` section's Plasma-restart block removed, but keep restarting `scribbleway-overlay` itself.)

- [ ] **Step 11: Full clean rebuild from scratch**

```bash
rm -rf build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

Expected: builds clean with no references to Plasma, `KF6::DBusAddons`, or `appletbackend`.

- [ ] **Step 12: Run the full test suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all remaining tests pass (the applet integration test is gone; everything else, including the new tests from Task 2 and Task 3, passes).

- [ ] **Step 13: Reinstall and manually verify end-to-end on the live machine**

```bash
./dev-reinstall.sh
```

Manual verification: the Plasma applet is gone from the panel (it was uninstalled — verify it doesn't linger in `plasmashell`'s widget list; a `systemctl --user restart plasma-plasmashell.service` or logout/login may be needed once to clear the stale entry, since `dev-reinstall.sh` no longer restarts Plasma). The **only** UI is the tray icon + popup from Task 5, and it still works exactly as verified there.

- [ ] **Step 14: Commit**

```bash
git add -A
git commit -m "refactor: remove Plasma applet and KDBusService, single-process tray UI only

Implements docs/adr/0001-retire-plasma-applet-unify-single-process-ui.md"
```

---

### Task 7: GitHub Actions CI workflow (build + test on push)

Linux only for now — there is no macOS backend until Phase 3, so a `macos-latest` job would just fail to build. Add it when Phase 3 lands.

**Files:**
- Create: `.github/workflows/ci.yml`

**Interfaces:** None — standalone infrastructure.

- [ ] **Step 1: Create the workflow**

```yaml
name: CI

on:
  push:
  pull_request:

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install build dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            cmake \
            extra-cmake-modules \
            libkf6globalaccel-dev \
            liblayershellqtinterface-dev \
            qt6-base-dev \
            qt6-declarative-dev \
            qml6-module-qtquick-layouts \
            qml6-module-qtquick-controls \
            qml6-module-qtquick-templates \
            qml6-module-qtquick-shapes \
            qml6-module-qtquick-effects

      - name: Configure
        run: cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

      - name: Build
        run: cmake --build build -j"$(nproc)"

      - name: Test
        run: ctest --test-dir build --output-on-failure
```

- [ ] **Step 2: Verify locally that the same dependency list builds cleanly in a clean environment**

```bash
docker run --rm -v "$(pwd)":/src -w /src ubuntu:24.04 bash -c "
  apt-get update && apt-get install -y cmake extra-cmake-modules libkf6globalaccel-dev liblayershellqtinterface-dev qt6-base-dev qt6-declarative-dev qml6-module-qtquick-layouts qml6-module-qtquick-controls qml6-module-qtquick-templates qml6-module-qtquick-shapes qml6-module-qtquick-effects &&
  cmake -B /tmp/build -S . -DCMAKE_BUILD_TYPE=Release &&
  cmake --build /tmp/build -j\$(nproc) &&
  ctest --test-dir /tmp/build --output-on-failure
"
```

Expected: succeeds. If a package name is wrong (Ubuntu package names shift between releases), fix the workflow's `apt-get install` list to match.

- [ ] **Step 3: Commit and push to trigger the real workflow**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: add GitHub Actions build+test workflow (Linux only)"
git push
```

Confirm on GitHub that the workflow run succeeds.

---

## Self-Review

**Spec coverage:**
- Kill Plasma applet, unify single-process tray UI everywhere on KDE — Tasks 4–6. ✓
- Tray Popup must never be fullscreen, must toggle on click — Task 5, with an explicit manual-verification gate rather than an unverified claim. ✓
- Port FullRepresentation.qml away from Kirigami/PlasmaComponents — done via fixing the already-existing leftover port (Task 4) rather than rewriting from scratch, since it was already Kirigami-free. ✓
- Replace KDBusService with QLocalServer everywhere — Tasks 3 and 6. ✓
- No OverlayPlatform virtual interface — explicitly not introduced; the leftover one is deleted in Task 1. ✓
- Repo cleanup (dead scripts/artifacts, keep only dev-reinstall.sh) — Task 1 (dead-only) and Task 6 (applet-dependent parts, sequenced after cutover). ✓
- GitHub Actions CI workflow — Task 7, scoped to Linux only since macOS backend doesn't exist yet (explicitly flagged, not silently over-promised). ✓
- Bundle SVG icons — Task 4 adds the 5 missing shape-type icons to the 14 that already existed. ✓
- ADR reference — linked in the Goal line and Task 6's commit message. ✓

**Explicitly out of scope for this plan** (per the Global Constraints and the phase-order decision): Hyprland validation (Phase 2), macOS backend (Phase 3), Windows, the release/packaging GitHub Actions workflow, `packaging/arch` updates, Wayland global hotkeys. These become their own plans once Phase 1 is merged and there's real findings to plan the next phase against.

**Placeholder scan:** no TBD/TODO markers; every code step shows real, complete code or an exact `git`/`grep`/`cmake` command.

**Type consistency:** `SingleInstanceGuard` (Task 3) is used identically in its test and in `main.cpp` (Task 6). `OverlayController`'s new property names (Task 2) match exactly what `FullRepresentation.qml` (Task 4) and `TrayPopup.qml` (Task 5) reference. `shapesMetadata` is consistently a property (not a method call) everywhere after Task 2.
