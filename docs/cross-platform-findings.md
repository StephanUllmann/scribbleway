# Cross-Platform Porting Findings

Date: 2025-07-25
Status: Research / Not yet planned

## Context

Scribbleway is a KDE Plasma / Wayland screen annotation overlay. We investigated what it would take to make it run on Windows, macOS, and non-KDE Linux desktops. The conclusion is that **the existing C++/Qt stack is the right choice** — no framework switch needed.

## Key Finding: The KDE-specific surface is tiny

The entire codebase (~265KB of C++/QML) depends on only **three** KDE/Linux-specific libraries, totaling roughly **70 lines** of platform-specific code in the overlay daemon:

| Dependency | Where | What it does | Lines |
|---|---|---|---|
| **LayerShellQt** | `main.cpp` (9–10, 23–24, 105–117), `overlaycontroller.cpp` (16, 773–777) | Creates transparent Wayland overlay window; toggles keyboard interactivity / input passthrough | ~20 |
| **KGlobalAccel** | `main.cpp` (11, 48, 70–71), `overlaycontroller.cpp` (20, 1006–1100) | System-wide hotkeys | ~40 |
| **KDBusService + D-Bus** | `main.cpp` (12–13, 32, 37–41) | Single-instance enforcement; applet↔daemon IPC | ~10 |

Everything else — the shapes model, QML rendering, roughness algorithm, Excalidraw serialization, undo/redo, clipboard, bindings, snapping — is pure Qt6 and standard C++20.

## Framework alternatives considered and rejected

We evaluated Go/Wails, Rust/Tauri, Electron, and Flutter as potential cross-platform rewrites. All were rejected because:

1. **The hard problem is the transparent overlay window with input passthrough.** No cross-platform framework provides this as a built-in. Tauri v2 has `set_ignore_cursor_events()`, but on Wayland it doesn't work (the compositor ignores it for `xdg_toplevel` surfaces — only `wlr-layer-shell` surfaces support input region control). Wails v2 doesn't even expose the API; Wails v3 (alpha) adds it but has the same Wayland limitation.

2. **Any framework switch means a full rewrite.** The existing QML rendering, shapes model, and controller logic would all be thrown away.

3. **Qt6 already runs on Windows, macOS, and Linux.** The rendering layer (Qt Quick Shapes, QML) is cross-platform by design. Only the ~70 lines of platform glue need per-OS backends.

## Overlay window: per-platform replacement for LayerShellQt

LayerShellQt does two things: (a) creates a fullscreen transparent overlay on the Wayland layer-shell, and (b) toggles input passthrough vs. capture. Each platform has native equivalents:

| Platform | Overlay creation | Input passthrough toggle |
|---|---|---|
| **Wayland/KDE** | Keep LayerShellQt as-is | `setKeyboardInteractivity()` on layer surface |
| **X11 (any DE)** | `Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint` + ARGB visual | `XShapeCombineRectangles` on input shape (compatible with existing `updateInputMask(QRegion)`) |
| **Windows** | `Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool` | `SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT)` via `QWindow::winId()` |
| **macOS** | `Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint`, then set `NSWindow.level = .screenSaver`, `NSWindow.isOpaque = false` | `NSWindow.ignoresMouseEvents = true/false` via `QWindow::winId()` → `NSView` → `window` |

Suggested abstraction (~30 lines per backend):

```cpp
class OverlayPlatform {
public:
    virtual ~OverlayPlatform() = default;
    virtual void setupOverlay(QQuickWindow *w) = 0;
    virtual void setInputPassthrough(QQuickWindow *w, bool passthrough) = 0;
    virtual void setKeyboardInteractivity(QQuickWindow *w, bool interactive) = 0;
    static std::unique_ptr<OverlayPlatform> create(); // compile-time factory
};
```

### GNOME/Wayland caveat

GNOME (Mutter) deliberately does not implement `wlr-layer-shell` and has no plans to. Overlay tools on GNOME/Wayland require either a GNOME Shell extension (JavaScript, separate project) or XWayland fallback. This is a political/architectural decision by GNOME, not a missing feature. Most annotation tools (Gromit-MPX, etc.) do not work on GNOME/Wayland either.

## Global hotkeys: replacement for KGlobalAccel

| Platform | Replacement |
|---|---|
| **KDE/Linux** | Keep KGlobalAccel (or migrate to `xdg-desktop-portal` GlobalShortcuts for broader Wayland support) |
| **Windows / macOS / X11** | [QHotkey](https://github.com/Skycoder42/QHotkey) — drop-in Qt library, same interface (register `QAction` + key sequence → `triggered()` signal) |

Compile-time `#ifdef` switch: KGlobalAccel on KDE, QHotkey elsewhere.

## Single-instance + IPC: replacement for KDBusService + D-Bus

| Platform | Replacement |
|---|---|
| **All** | `QLocalServer` / `QLocalSocket` (built into Qt, cross-platform) for single-instance check and IPC. Or: eliminate the two-process split entirely (see Applet section below). |

## Applet: the Plasma widget → system tray popup

The Plasma applet consists of:

- **`AppletBackend` (C++, ~490 lines in `src/applet-plugin/`)** — a D-Bus client bridging the Plasma applet process to the overlay daemon.
- **`FullRepresentation.qml` (~905 lines)** — the popup UI with tool buttons, color pickers, sliders (width, opacity, glow, roughness, smoothing, border radius, fill), font controls, shape list manager, and keyboard shortcuts editor.
- **`CompactRepresentation.qml` (~31 lines)** — the tray icon with a connection status dot.
- **`main.qml` (~38 lines)** — `PlasmoidItem` wrapper with `Plasmoid.configuration` for persisting target screen.

### Why the two-process architecture exists (and can be eliminated)

The `AppletBackend` D-Bus bridge exists only because Plasma applets run inside the Plasma shell process, separate from the overlay daemon. On non-KDE platforms, the settings popup would live **in the same process** as the overlay daemon. This means:

- Delete `AppletBackend` entirely (~490 lines + ~140 lines header)
- The settings QML talks directly to `OverlayController` via QML context property
- D-Bus IPC disappears — net code reduction

### QML component replacement map

The applet QML uses KDE components for styling only. All logic and layout is standard QML. The replacement is mechanical:

| KDE type | Qt Quick Controls replacement |
|---|---|
| `PlasmaComponents.Button` | `Controls.Button` (already imported in the file) |
| `PlasmaComponents.Label` | `Controls.Label` or `Text` |
| `PlasmaComponents.Slider` | `Controls.Slider` |
| `PlasmaComponents.ToolButton` | `Controls.ToolButton` |
| `PlasmaComponents.ItemDelegate` | `Controls.ItemDelegate` |
| `PlasmaExtras.Heading` | `Text { font.pointSize: 14; font.bold: true }` |
| `Kirigami.Units.gridUnit` | Constant singleton (`QtObject { readonly property int gridUnit: 18 }`) |
| `Kirigami.Units.smallSpacing` | `4` (literal or from singleton) |
| `Kirigami.Units.largeSpacing` | `8` |
| `Kirigami.Units.iconSizes.*` | Literal pixel values |
| `Kirigami.Theme.highlightColor` | `palette.highlight` (Qt Quick native palette) |
| `Kirigami.Theme.disabledTextColor` | `palette.disabled.text` |
| `Kirigami.Theme.hoverColor` | `palette.mid` or custom |
| `Kirigami.Theme.warningColor` | Hardcoded `"#f39c12"` or similar |
| `Kirigami.Theme.backgroundColor` | `palette.base` |
| `Kirigami.Icon` | `Image` with bundled SVG icons |
| `PlasmoidItem` | `ApplicationWindow` or `Controls.Popup` |
| `Plasmoid.configuration` | `QSettings` |

`Controls.ComboBox`, `Controls.ScrollView`, and `QtQuick.Dialogs.ColorDialog` are already used in the file and are plain Qt — no changes needed.

### Icons

KDE icon names (`"draw-freehand"`, `"edit-undo"`, `"edit-delete"`, etc.) come from the Freedesktop icon spec. These don't exist on Windows/macOS. Simplest fix: bundle ~20 SVG icons via Qt resources (`qrc`).

## Summary: what stays, what changes

```
STAYS UNCHANGED (pure Qt, cross-platform):
  src/overlay/overlaycontroller.{h,cpp}     ~114KB  shapes, selection, undo, clipboard, Excalidraw, bindings
  src/overlay/shapesmodel.{h,cpp}           ~14KB   list model
  src/overlay/qml/main.qml                  ~33KB   overlay rendering (all QML Shapes / Qt Quick)
  src/overlay/qml/shapes/*                          all shape components + RoughPathGenerator.js
  src/overlay/qml/ToolCursorBadge.qml       ~5KB    cursor badge
  tests/shapesmodeltest.cpp                 ~114KB  unit tests

NEEDS PLATFORM BACKENDS (~30 lines each, 4 backends):
  LayerShellQt calls in main.cpp                    → OverlayPlatform abstraction
  LayerShellQt calls in overlaycontroller.cpp       → OverlayPlatform abstraction
  KGlobalAccel in main.cpp + overlaycontroller.cpp  → QHotkey + compile switch

NEEDS MECHANICAL QML REWRITE (find-and-replace):
  applet/contents/ui/FullRepresentation.qml  ~905 lines  PlasmaComponents → Controls
                                                          Kirigami.* → palette + constants

GETS DELETED (simplification from single-process):
  src/applet-plugin/appletbackend.{h,cpp}    ~630 lines  D-Bus bridge no longer needed
  applet/contents/ui/main.qml                ~38 lines   PlasmoidItem wrapper
  applet/metadata.json                                    Plasma metadata

GETS ADDED:
  QSystemTrayIcon setup + popup window host  ~50 lines
  Units/theme constants singleton            ~10 lines
  Bundled SVG icons                          ~20 files, few KB
  QSettings for persisted preferences        ~20 lines
  QLocalServer for single-instance           ~30 lines
```
