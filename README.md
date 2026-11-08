# Scribbleway

A modern, Excalidraw-style screen annotation utility designed for **Wayland**, on KDE Plasma 6 and other layer-shell compositors.

Scribbleway allows you to draw shapes, lines, arrows, and text directly over your screen, providing a rich, interactive overlay for presentations, demonstrations, or general desktop markup.

---

## Key Features

- **Excalidraw-Style Drawing Tools**: Freehand brush, lines, arrows, rectangles, ellipses, and text.
- **Live Shape Manipulation**: Select, move, resize, layer-reorder (raise/lower), lock, or delete shapes after drawing them.
- **Rich Property Controls**: Adjust stroke colors (preset palettes or custom pickers), width, and opacity per shape or globally.
- **Wayland Overlay Layer**: Built on `LayerShellQt` to provide full-screen coverage that handles keyboard and pointer focus transparently.
- **Tray Popup**: A system tray popup for tools, properties and the list of drawn shapes.
- **Global Hotkey**: `Meta+Shift+X` through KDE `KGlobalAccel`, or `scribbleway-overlay --toggle` on other compositors.

---

## Repository Structure

- **`src/overlay/`**: The single `scribbleway-overlay` binary: the C++/Qt engine (`OverlayController`, `ShapesModel`, the Excalidraw clipboard translation) and the QML overlay and tray popup.
- **`tests/`**: C++ and QML tests for shape handling, undo, the clipboard format and the overlay QML.
- **`packaging/`, `debian/`**: Arch, Flatpak and Debian packaging.

---

## Build & Installation

### Requirements & Dependencies

**Qt 6.6 or newer and KDE Frameworks 6 are required.** Scribbleway runs on KDE Plasma 6 and on other Wayland compositors with layer-shell support, such as Hyprland.

Install the appropriate system package dependencies for your distribution below:

#### Arch Linux
```bash
sudo pacman -S extra-cmake-modules layer-shell-qt kglobalaccel qt6-declarative qt6-base
```

#### Fedora
```bash
sudo dnf install extra-cmake-modules kf6-kglobalaccel-devel layer-shell-qt-devel qt6-qtbase-devel qt6-qtdeclarative-devel
```

#### Ubuntu / Debian / KDE Neon
```bash
sudo apt install extra-cmake-modules libkf6globalaccel-dev liblayershellqtinterface-dev qt6-base-dev qt6-declarative-dev
```

### Installation

To build and install under `~/.local`:

```bash
cmake -B build -S . -DCMAKE_INSTALL_PREFIX="$HOME/.local" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cmake --install build
```

For development, `./dev-reinstall.sh` removes the old install, rebuilds, installs and restarts the overlay.

To uninstall, remove the files CMake recorded:

```bash
xargs rm < build/install_manifest.txt
```

### Packaging

#### Arch Linux (AUR)
A `PKGBUILD` is provided under `packaging/arch/` for packaging Scribbleway as an AUR package. To build and install:
```bash
cd packaging/arch
makepkg -si
```

#### Debian / Ubuntu
Standard Debian packaging files are located in the `debian/` directory. To build the `.deb` package:
```bash
dpkg-buildpackage -us -uc -b
```

#### Flatpak
Prerequisites: `flatpak`, `flatpak-builder`, and Flathub access.

```bash
./packaging/build_flatpak.sh           # build and install for current user
./packaging/build_flatpak.sh --bundle  # also produce scribbleway.flatpak
flatpak run org.kde.scribbleway
```

The global shortcut (`Meta+Shift+X`) needs the host's `plasma-kglobalaccel`. Autostart is not provided by the Flatpak; start the daemon manually or keep the native autostart desktop file from a host install.

### Running the Daemon

If `$HOME/.local/bin` is in your `PATH`, you can launch the overlay helper daemon using:
```bash
scribbleway-overlay
```

Or manually from your build tree:
```bash
./build/src/overlay/scribbleway-overlay
```

To run unit tests:
```bash
ctest --test-dir build --output-on-failure
```

### Running on Hyprland (or other non-Plasma Wayland compositors)

Scribbleway's controls live in a system tray popup, which requires a running
`StatusNotifierHost`. Hyprland doesn't provide one itself — enable Waybar's
`tray` module (or an equivalent) before launching Scribbleway, or the tray
icon won't appear anywhere.

The global `Meta+Shift+X` shortcut goes through KDE's `KGlobalAccel`, which does
nothing without Plasma's `kglobalacceld`. Bind the compositor's own hotkey to
`--toggle` instead — it hands the request to the running daemon over the
single-instance socket, and starts it if it isn't running yet:

```lua
-- ~/.config/hypr/hyprland.lua
hl.bind("SUPER + SHIFT + X", hl.dsp.exec_cmd("scribbleway-overlay --toggle"))
```

Hyprland switched its config language from hyprlang to Lua in 0.55. A legacy
`hyprland.conf` is still read when no `hyprland.lua` exists, but upstream plans
to drop it — on such a setup the equivalent line is:

```
bind = SUPER SHIFT, X, exec, scribbleway-overlay --toggle
```

Other compositors bind the same command: `sway`/`niri`/`river` all take an
`exec`-style spawn.

The overlay-local hotkeys in the table below (`E`, `R`, `X`, `+`/`-`, …) work
once a mode is active — the overlay takes exclusive keyboard focus while a tool
is in use and releases it in passthrough mode.

---

## Default Keyboard Shortcuts

Scribbleway uses a two-tier modal shortcut model. A single global shortcut opens the overlay, after which you can use simple unmodified keys to activate tools:

### Global Shortcut
- Toggle Overlay: `Meta` + `Shift` + `X` (Enters select mode or returns to passthrough).

### Overlay Local Hotkeys (active when overlay is open)
| Tool / Action | Hotkey |
| :--- | :--- |
| **Draw Arrow** | `A` |
| **Draw Rectangle** | `R` |
| **Draw Freehand** | `F` |
| **Draw Ellipse** | `E` |
| **Draw Line** | `L` |
| **Draw Text** | `T` |
| **Select Mode** | `X` |
| **Cycle Roughness** | `S` |
| **Cycle Preset Color** | `Q` |
| **Grow Selected (Size/Stroke)**| `+` |
| **Shrink Selected (Size/Stroke)**| `-` |
| **Undo Last Shape** | `Ctrl` + `Z` |
| **Clear All Shapes** | `Ctrl` + `Delete` |
| **Exit Overlay (Passthrough)** | `Escape` |

There is no screen for changing hotkeys. To override a local one, add it to `~/.config/scribbleway/shortcuts.conf`, for example `tool_arrow=W` under a `[local]` section, and restart the overlay.
