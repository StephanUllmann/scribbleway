# Phase 3: macOS Backend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Get `scribbleway-overlay` building and running correctly on macOS — transparent overlay window, click-through toggling, global hotkey, and the Tray Popup from Phase 1 — using a real MacBook for verification, not speculation.

**Architecture:** No virtual `OverlayPlatform` interface (Q7). A small pair of free functions (`setupOverlayMac`, declared in `src/overlay/platform_mac.h`, implemented in an Objective-C++ `.mm` file) replace the `LayerShellQt` calls, called from `main.cpp` behind `#if defined(Q_OS_MACOS)`. Global hotkeys use `QHotkey` (vendored as a git submodule, not `FetchContent` — see Task 2) instead of `KGlobalAccel`, since KDE Frameworks doesn't exist on macOS at all. `KF6::GlobalAccel` and `LayerShellQt` become Linux-only `find_package` calls in the root `CMakeLists.txt`.

**Tech Stack:** C++20, Objective-C++ (`.mm`, macOS-only), Qt6 (Core, Gui, Widgets, Qml, Quick, DBus), QHotkey (git submodule), CMake with `OBJCXX` language support.

## Global Constraints

- Assumes Phase 1 is fully merged. Assumes Phase 2 is merged or at least that its Arch-packaging changes don't matter here — this plan only touches the parts of `CMakeLists.txt` that are Linux-vs-macOS conditional.
- No Windows work in this plan.
- **`.dmg`/`.app` distribution packaging and the release GitHub Actions workflow are explicitly out of scope.** This plan's finish line is "builds and runs correctly from a local dev build on the MacBook" — packaging becomes its own later plan once this one's findings exist.
- No `OverlayPlatform` virtual interface (Q7) — plain functions, `#ifdef`-selected at compile time.
- **This plan contains one genuinely open technical question, called out explicitly rather than papered over**: whether `QWindow::setMask(QRegion)` — the mechanism the existing Linux code uses for *all* click-through, including letting the user click already-drawn shapes while otherwise in passthrough mode (`OverlayController::updateInputMask`) — behaves as true hit-test click-through on macOS's Cocoa platform plugin, or only affects visual clipping. The original research behind `docs/cross-platform-findings.md` recommended `NSWindow.ignoresMouseEvents` for macOS specifically instead of relying on `setMask`, which is itself evidence that `setMask` alone may not be trustworthy there. Task 4 investigates this on real hardware before deciding the final design — don't treat the code in Task 4 as more certain than it is.

---

## File Structure

**Create:**
- `extern/QHotkey/` — git submodule (`Skycoder42/QHotkey`).
- `src/overlay/platform_mac.h` — free-function declarations, portable (safe to include on any platform; bodies compiled only on `APPLE`).
- `src/overlay/platform_mac.mm` — Objective-C++ implementation, only compiled when `APPLE`.
- `packaging/macos/Info.plist.in` — bundle metadata template (`LSUIElement`, bundle identifier).

**Modify:**
- `CMakeLists.txt` (root) — guard `KF6::GlobalAccel`/`LayerShellQt` to `if(NOT APPLE)`; add `QHotkey` submodule + `OBJCXX` on `APPLE`; set macOS bundle properties.
- `src/overlay/CMakeLists.txt` — platform-conditional sources (`platform_mac.mm` only on `APPLE`) and link libraries.
- `src/overlay/main.cpp` — `#if defined(Q_OS_LINUX)` / `#elif defined(Q_OS_MACOS)` split for the overlay setup block and the global hotkey.
- `src/overlay/overlaycontroller.cpp` — same split inside `setKeyboardInteractivity()`.
- `.github/workflows/ci.yml` — add a `macos-latest` job (build-only; it can't interactively verify tray/overlay behavior, but it catches compile regressions on every future push).

---

### Task 1: Conditional CMake — Linux-only KF6/LayerShellQt, macOS QHotkey + Objective-C++

This task's correctness can be reasoned about and reviewed without a Mac (it's CMake logic), but its *build* can only be confirmed on Task 4/6 once there's an actual macOS toolchain to run it against.

**Files:**
- Create: `extern/QHotkey/` (git submodule)
- Modify: `CMakeLists.txt` (root)

**Interfaces:**
- Produces: the `QHotkey::QHotkey` CMake target, available only when `APPLE` (Task 2 links against it).

- [ ] **Step 1: Vendor QHotkey as a git submodule**

`FetchContent` is known not to work cleanly for Qt-integrated CMake libraries like this one (Qt's CMake integration doesn't compose well with `FetchContent`-fetched sources — see Sources). A git submodule + `add_subdirectory` is the robust pattern:

```bash
git submodule add https://github.com/Skycoder42/QHotkey.git extern/QHotkey
git submodule update --init --recursive
```

- [ ] **Step 2: Guard `KF6::GlobalAccel` and `LayerShellQt` to Linux, add QHotkey on macOS**

In the root `CMakeLists.txt`, replace:

```cmake
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    GlobalAccel
)

find_package(LayerShellQt REQUIRED)
```

with:

```cmake
if(NOT APPLE)
    find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
        GlobalAccel
    )
    find_package(LayerShellQt REQUIRED)
endif()

if(APPLE)
    enable_language(OBJCXX)
    set(QT_DEFAULT_MAJOR_VERSION 6)
    set(QHOTKEY_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(extern/QHotkey)
endif()
```

(`enable_language(OBJCXX)` must be called at the top level, before `add_subdirectory(src/overlay)`, for CMake to know how to compile `.mm` files in that subdirectory.)

- [ ] **Step 3: Commit**

```bash
git add .gitmodules extern/QHotkey CMakeLists.txt
git commit -m "build: vendor QHotkey submodule, guard KF6/LayerShellQt to Linux-only"
```

---

### Task 2: Global hotkey via QHotkey on macOS

The current Linux code only actually wires **one** real global hotkey — `Meta+Shift+X`, toggling select/passthrough mode (`actionActivateSelect` in `main.cpp`). The rest of `cleanLegacy`'s action-ID list is dead cleanup code for shortcuts that were migrated to in-QML local shortcuts years ago — it has nothing to do with macOS and isn't touched by this task.

**Files:**
- Modify: `src/overlay/main.cpp`
- Modify: `src/overlay/CMakeLists.txt`

**Interfaces:**
- Consumes: `QHotkey::QHotkey` (Task 1), `OverlayController::currentMode()`, `enterSelectMode()`, `enterPassthroughMode()` (all existing, unchanged).

- [ ] **Step 1: Split the includes**

Replace:

```cpp
#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>
#include <KGlobalAccel>
#include <QDBusConnection>
```

(the last two were already partially removed by Phase 1's Task 6 — confirm current state first) with:

```cpp
#if defined(Q_OS_LINUX)
#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>
#include <KGlobalAccel>
#elif defined(Q_OS_MACOS)
#include <QHotkey>
#include "platform_mac.h"
#endif
```

- [ ] **Step 2: Split `LayerShellQt::Shell::useLayerShell()`**

```cpp
#if defined(Q_OS_LINUX)
    LayerShellQt::Shell::useLayerShell();
#endif
```

- [ ] **Step 3: Split the hotkey setup**

Wrap the existing `cleanLegacy` lambda, `setupGlobalAction` lambda, and `actionActivateSelect` block in `#if defined(Q_OS_LINUX) ... #endif`. Add the macOS equivalent immediately after:

```cpp
#elif defined(Q_OS_MACOS)
    QHotkey macSelectHotkey(QKeySequence(QStringLiteral("Meta+Shift+X")), true, &app);
    QObject::connect(&macSelectHotkey, &QHotkey::activated, &controller, [&controller]() {
        if (controller.currentMode() == QStringLiteral("passthrough")) {
            controller.enterSelectMode();
        } else {
            controller.enterPassthroughMode();
        }
    });
#endif
```

(Note: `Qt::Key_Meta` maps to the Command key on macOS in Qt's key-sequence parsing, so `"Meta+Shift+X"` is Cmd+Shift+X on macOS, not literally the same physical key as KDE's Meta/Super key on Linux — this is standard Qt cross-platform behavior, not a bug, but worth knowing when testing on the MacBook in Task 6.)

- [ ] **Step 4: Link QHotkey conditionally**

In `src/overlay/CMakeLists.txt`, add:

```cmake
if(APPLE)
    target_sources(scribbleway-overlay PRIVATE platform_mac.mm)
    target_link_libraries(scribbleway-overlay PRIVATE QHotkey::QHotkey)
endif()
if(NOT APPLE)
    target_link_libraries(scribbleway-overlay PRIVATE KF6::GlobalAccel LayerShellQt::Interface)
endif()
```

(Move the existing unconditional `KF6::GlobalAccel` and `LayerShellQt::Interface` entries out of the main `target_link_libraries(scribbleway-overlay PRIVATE Qt6::Quick Qt6::Qml Qt6::DBus ...)` call into this conditional block.)

- [ ] **Step 5: Commit**

```bash
git add src/overlay/main.cpp src/overlay/CMakeLists.txt
git commit -m "feat(macos): wire the select-mode global hotkey via QHotkey"
```

(This won't compile yet on Linux either, until Task 3 provides `platform_mac.h` for the `#include` on the macOS branch — Task 3 must land before this is buildable on any platform. Treat Tasks 1–3 as landing together in practice even though they're separated for review granularity.)

---

### Task 3: `platform_mac.h` / `platform_mac.mm` — overlay window setup

Grounded in the confirmed Qt6/Cocoa pattern: `QWindow::winId()` returns an `NSView*` on macOS; `[nsView window]` gets the owning `NSWindow*`. This must live in a `.mm` file — `reinterpret_cast`+`__bridge` between C++ and Objective-C types isn't valid in a plain `.cpp`/`.h`.

**Files:**
- Create: `src/overlay/platform_mac.h`
- Create: `src/overlay/platform_mac.mm`

**Interfaces:**
- Produces: `void setupOverlayMac(QQuickWindow *window);` — called once, when the overlay window is first created (Task 4).

- [ ] **Step 1: Write the header**

```cpp
// src/overlay/platform_mac.h
#pragma once

#include <QQuickWindow>

void setupOverlayMac(QQuickWindow *window);
```

- [ ] **Step 2: Write the implementation**

```objc++
// src/overlay/platform_mac.mm
#include "platform_mac.h"

#import <Cocoa/Cocoa.h>

void setupOverlayMac(QQuickWindow *window)
{
    if (!window) return;

    window->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    window->setColor(Qt::transparent);

    NSView *nsView = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    NSWindow *nsWindow = [nsView window];
    if (!nsWindow) return;

    nsWindow.level = NSScreenSaverWindowLevel;
    nsWindow.opaque = NO;
    nsWindow.backgroundColor = [NSColor clearColor];
    nsWindow.hasShadow = NO;
    nsWindow.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
                                 | NSWindowCollectionBehaviorStationary
                                 | NSWindowCollectionBehaviorFullScreenAuxiliary;
}
```

`nsWindow.collectionBehavior` is new relative to the original findings doc — without it, a fullscreen-transparent overlay on macOS typically doesn't follow the user across Spaces/fullscreen apps, which would make the overlay effectively useless outside whatever Space it was created on. This needs confirming on real hardware in Task 6, not assumed correct from documentation alone.

- [ ] **Step 3: Commit**

```bash
git add src/overlay/platform_mac.h src/overlay/platform_mac.mm
git commit -m "feat(macos): NSWindow-level overlay setup via QWindow::winId() bridge"
```

---

### Task 4: Wire `setupOverlayMac` into `main.cpp`; investigate input-passthrough fidelity

**This task has a real open question — read the Global Constraints section above before starting.** Steps 1–2 are confident. Step 3 is an investigation, not a known-correct implementation.

**Files:**
- Modify: `src/overlay/main.cpp`
- Modify: `src/overlay/overlaycontroller.cpp`

**Interfaces:**
- Consumes: `setupOverlayMac()` (Task 3).

- [ ] **Step 1: Split the window-created callback's platform setup**

In `main.cpp`'s `engine.load(url)` callback, replace the `LayerShellQt`-only block:

```cpp
            // Apply LayerShell configuration
            if (auto *layerWindow = LayerShellQt::Window::get(window)) {
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
            }
```

with:

```cpp
#if defined(Q_OS_LINUX)
            if (auto *layerWindow = LayerShellQt::Window::get(window)) {
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
            }
#elif defined(Q_OS_MACOS)
            setupOverlayMac(window);
            window->setGeometry(QGuiApplication::primaryScreen()->geometry());
#endif
```

(`setGeometry` to the full screen replaces what `LayerShellQt`'s anchors did on Wayland — macOS has no layer-shell equivalent, so the overlay is just a regular full-screen-sized borderless window at `NSScreenSaverWindowLevel`.)

- [ ] **Step 2: Split `OverlayController::setKeyboardInteractivity`**

In `overlaycontroller.cpp`, the existing body:

```cpp
void OverlayController::setKeyboardInteractivity(bool interactive)
{
    if (!m_window) return;
    if (auto *layerWindow = LayerShellQt::Window::get(m_window)) {
        layerWindow->setKeyboardInteractivity(interactive
            ? LayerShellQt::Window::KeyboardInteractivityOnDemand
            : LayerShellQt::Window::KeyboardInteractivityNone);
    }
}
```

becomes:

```cpp
void OverlayController::setKeyboardInteractivity(bool interactive)
{
    if (!m_window) return;
#if defined(Q_OS_LINUX)
    if (auto *layerWindow = LayerShellQt::Window::get(m_window)) {
        layerWindow->setKeyboardInteractivity(interactive
            ? LayerShellQt::Window::KeyboardInteractivityOnDemand
            : LayerShellQt::Window::KeyboardInteractivityNone);
    }
#elif defined(Q_OS_MACOS)
    if (interactive) {
        m_window->requestActivate();
    }
#endif
}
```

`LayerShellQt`'s "keyboard interactivity" is a Wayland layer-shell-protocol concept (whether the surface may take keyboard focus at all) with no macOS equivalent — regular `NSWindow`/`QWindow` activation via `requestActivate()` covers the same practical need (letting the overlay receive key events when the user is actively drawing/typing text).

- [ ] **Step 3: Investigate `setMask` click-through fidelity on macOS — real hardware required**

`OverlayController::updateInputMask(const QVariantList &rects)` (unchanged, still calls `m_window->setMask(region)`) is what makes already-drawn shapes clickable while the rest of the screen stays click-through. It is **not** modified by this task, on the theory that `QWindow::setMask()` is documented as cross-platform Qt API and might just work identically via the Cocoa QPA plugin.

On the MacBook (this step depends on Task 6's build existing):

1. Launch the overlay, enter select mode, draw a shape, return to passthrough mode.
2. Click where the shape is — does clicking select/interact with it (correct), or does the click pass through to whatever's behind the overlay regardless (broken)?
3. Click empty overlay area away from any shape — does it pass through to the app behind (correct), or does the whole window still eat the click (broken — this would mean `setMask` on Cocoa is a visual-only clip, not a real hit-test mask, matching the concern raised in Global Constraints)?

**If it works:** no code change needed — document the finding in this plan's file (edit this task's checkbox notes) and move on.

**If it's broken:** the fallback, per the original findings doc, is `NSWindow.ignoresMouseEvents`. This is a coarser mechanism (whole-window boolean, no per-region mask), so it can correctly replicate the *coarse* passthrough-vs-capture toggle (add a `setInputPassthroughMac(QQuickWindow*, bool)` function to `platform_mac.h`/`.mm` following the same `winId()`-bridge pattern as `setupOverlayMac`, call it from wherever `enterSelectMode()`/`enterPassthroughMode()` change mode), but **cannot** replicate "click already-drawn shapes while otherwise passthrough" — that would need a different design (e.g., a small always-on-top native hit-test proxy, or accepting the UX regression on macOS specifically). Don't attempt that redesign inside this task if it comes to it — stop, record the finding, and treat it as a new decision point requiring a conversation with the user rather than a unilateral design call embedded in a plan step.

- [ ] **Step 4: Commit whatever Step 3 concludes**

```bash
git add src/overlay/main.cpp src/overlay/overlaycontroller.cpp
git commit -m "feat(macos): wire overlay window setup and keyboard activation"
```

(If Step 3 required the `ignoresMouseEvents` fallback, include that in the same commit with a message reflecting what was actually found, not this plan's guess.)

---

### Task 5: App bundle metadata (`LSUIElement`, bundle identifier)

Without this, the built app shows a Dock icon and a menu bar — wrong for a tray-only utility, and inconsistent with "should look and feel like a Plasma applet" from the grilling session.

**Files:**
- Create: `packaging/macos/Info.plist.in`
- Modify: `CMakeLists.txt` (root)

**Interfaces:** None.

- [ ] **Step 1: Create the plist template**

```xml
<!-- packaging/macos/Info.plist.in -->
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>Scribbleway</string>
    <key>CFBundleIdentifier</key>
    <string>io.github.stephanullmann.scribbleway</string>
    <key>CFBundleVersion</key>
    <string>${MACOSX_BUNDLE_BUNDLE_VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${MACOSX_BUNDLE_SHORT_VERSION_STRING}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSMinimumSystemVersion</key>
    <string>${CMAKE_OSX_DEPLOYMENT_TARGET}</string>
    <key>LSUIElement</key>
    <true/>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
```

`LSUIElement = true` is the key line — it's the standard macOS mechanism for "accessory" apps that live only in the menu bar/tray with no Dock icon and no application menu, exactly matching the Tray Popup's intended feel.

- [ ] **Step 2: Wire it up for macOS builds**

In the root `CMakeLists.txt`, inside the `if(APPLE)` block added in Task 1:

```cmake
if(APPLE)
    enable_language(OBJCXX)
    set(QT_DEFAULT_MAJOR_VERSION 6)
    set(QHOTKEY_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(extern/QHotkey)

    set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
endif()
```

In `src/overlay/CMakeLists.txt`, inside the existing `if(APPLE)` block from Task 2:

```cmake
if(APPLE)
    target_sources(scribbleway-overlay PRIVATE platform_mac.mm)
    target_link_libraries(scribbleway-overlay PRIVATE QHotkey::QHotkey)
    set_target_properties(scribbleway-overlay PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in
    )
endif()
```

- [ ] **Step 3: Commit**

```bash
git add packaging/macos/Info.plist.in CMakeLists.txt src/overlay/CMakeLists.txt
git commit -m "feat(macos): bundle as an LSUIElement accessory app (no Dock icon)"
```

---

### Task 6: Build and verify on the real MacBook

This is the task that actually answers whether Tasks 1–5 work. Nothing before this point has been run on macOS.

**Files:** None — verification only. Fixes discovered here become their own follow-up commits via `superpowers:systematic-debugging`, not pre-written here.

- [ ] **Step 1: Install prerequisites (Homebrew)**

```bash
brew install qt cmake
```

- [ ] **Step 2: Configure and build**

```bash
git submodule update --init --recursive
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j"$(sysctl -n hw.ncpu)"
```

Expected: succeeds. If it doesn't, this is the first real signal about what in Tasks 1–5 was wrong on paper — fix forward with a fresh commit, don't silently amend history.

- [ ] **Step 3: Run and verify against a checklist**

```bash
./build/bin/scribbleway-overlay.app/Contents/MacOS/scribbleway-overlay
```

- [ ] No Dock icon appears, no menu bar for the app (confirms `LSUIElement`).
- [ ] A tray icon appears in the macOS menu bar.
- [ ] Clicking it opens the Tray Popup, non-fullscreen, at the expected size and position.
- [ ] The overlay window is transparent and covers the full screen.
- [ ] Switching to another Space, or entering a fullscreen app, and back — does the overlay persist correctly? (Tests the `collectionBehavior` flags from Task 3.)
- [ ] Cmd+Shift+X toggles select/passthrough mode (Task 2's `QHotkey` wiring).
- [ ] Run Task 4 Step 3's click-through investigation now and record the outcome.
- [ ] Draw a shape, undo/redo/clear from the popup, confirm they all work.

- [ ] **Step 4: Record findings**

Whatever fails, open a `superpowers:systematic-debugging` pass per failure. Once everything passes, this plan is done — `.dmg` packaging and CI are separate remaining tasks (7, below) and a future plan, respectively.

---

### Task 7: Add a `macos-latest` job to CI

Build-only — it can't click a tray icon or verify Space-switching, but it catches every future compile regression on macOS automatically, which nothing before this point in the project has ever done.

**Files:**
- Modify: `.github/workflows/ci.yml`

**Interfaces:** None.

- [ ] **Step 1: Add the job**

```yaml
  build-macos:
    runs-on: macos-14
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install Qt
        run: brew install qt

      - name: Configure
        run: cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"

      - name: Build
        run: cmake --build build -j"$(sysctl -n hw.ncpu)"
```

(No `ctest` step — `tests/CMakeLists.txt` currently only builds `shapesmodeltest`, which links `KF6::GlobalAccel` and `LayerShellQt::Interface` unconditionally per Phase 1. Making the test target itself cross-platform is out of scope for this plan; note it as a gap for whoever picks up the test suite next, not something to silently skip fixing without flagging.)

- [ ] **Step 2: Commit and push to confirm the job runs**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: add macOS build job"
git push
```

---

## Self-Review

**Spec coverage:**
- macOS backend using `NSWindow`/`QWindow::winId()` bridge, per the mapping table in the findings doc — Task 3, grounded in verified real-world Qt/Cocoa examples (see Sources), not reconstructed from memory alone. ✓
- Hotkeys via QHotkey (works on macOS, unlike its Linux/X11 backend) — Task 2. ✓
- No `OverlayPlatform` virtual interface — Tasks 3–4 use plain functions. ✓
- `.dmg`/packaging explicitly deferred, not silently expanded into scope — stated in Global Constraints and Task 6's closing note. ✓
- Real hardware verification, not assumed-correct-on-paper — Task 6, plus Task 4's explicit open question about `setMask` fidelity. ✓

**Explicitly out of scope:** `.dmg`/`.app` distribution packaging, the release GitHub Actions workflow, Windows, making `tests/shapesmodeltest` buildable on macOS (flagged as a gap, not fixed).

**Placeholder scan:** clean, except Task 4 Step 3, which is *deliberately* an investigation rather than a pre-decided fix — flagged explicitly as such rather than disguised as certain code, per the Global Constraints note about not overclaiming certainty on the one part of this plan that's genuinely unverified.

**Type consistency:** `setupOverlayMac(QQuickWindow*)` (Task 3) is declared once in `platform_mac.h` and called identically in Task 4. `QHotkey::QHotkey` target name and `#include <QHotkey>` header path verified against the library's actual `CMakeLists.txt`/README rather than assumed.

Sources for the macOS-specific technical claims in this plan:
- [qt-macos-nswindow-config (msorvig)](https://github.com/msorvig/qt-macos-nswindow-config) — `QWindow::winId()` → `NSView*` → `NSWindow*` bridge pattern.
- [QHotkey (Skycoder42)](https://github.com/Skycoder42/QHotkey) and its `CMakeLists.txt`/`README.md` — target name `QHotkey::QHotkey`, `QT_DEFAULT_MAJOR_VERSION`, constructor/signal API.
- [QHotkey issue #24 — CMake not working with FetchContent](https://github.com/Skycoder42/QHotkey/issues/24) — basis for using a git submodule instead of `FetchContent`.
