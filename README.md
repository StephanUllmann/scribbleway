# Scribbleway

A modern, Excalidraw-style screen annotation utility designed for **Wayland** and **KDE Plasma 6**.

Scribbleway allows you to draw shapes, lines, arrows, and text directly over your screen, providing a rich, interactive overlay for presentations, demonstrations, or general desktop markup.

---

## Key Features

- **Excalidraw-Style Drawing Tools**: Freehand brush, lines, arrows, rectangles, ellipses, and text.
- **Live Shape Manipulation**: Select, move, resize, layer-reorder (raise/lower), lock, or delete shapes after drawing them.
- **Rich Property Controls**: Adjust stroke colors (preset palettes or custom pickers), width, and opacity per shape or globally.
- **Wayland Overlay Layer**: Built on `LayerShellQt` to provide full-screen coverage that handles keyboard and pointer focus transparently.
- **KDE Plasma 6 Widget**: A companion Plasmoid applet for easy status viewing, drawn shape management, and property adjustments.
- **Decoupled D-Bus Architecture**: A clean D-Bus interface connects the applet plugin (`AppletBackend`) to the overlay daemon (`OverlayController`).
- **Customizable Global Hotkeys**: Configured through KDE `KGlobalAccel` and adjustable directly from the Plasmoid UI.

---

## Repository Structure

- **[src/overlay/](file:///home/stephan/coding/projects/scribbleway/src/overlay)**: The main drawing overlay daemon (`scribbleway-overlay`). Contains the C++/Qt engine (`OverlayController`, `ShapesModel`) and the QML overlay elements.
- **[src/applet-plugin/](file:///home/stephan/coding/projects/scribbleway/src/applet-plugin)**: The C++ bridge backend (`AppletBackend`) registering D-Bus communication to expose the overlay status and actions to the widget.
- **[applet/](file:///home/stephan/coding/projects/scribbleway/applet)**: The KDE Plasma 6 desktop widget UI and metadata configurations.
- **[tests/](file:///home/stephan/coding/projects/scribbleway/tests)**: Automated unit tests covering core drawing and shape management logic (`ShapesModel`).

---

## Build & Installation

### Requirements & Dependencies

**KDE Plasma 6 and Qt 6 are strictly required.** Scribbleway is built specifically for Qt 6 and KDE Plasma 6; older versions (such as Qt 5 or Plasma 5) are not supported.

Install the appropriate system package dependencies for your distribution below:

#### Arch Linux
```bash
sudo pacman -S extra-cmake-modules plasma-desktop layer-shell-qt kglobalaccel kdbusaddons qt6-declarative qt6-base
```

#### Fedora
```bash
sudo dnf install extra-cmake-modules kf6-kglobalaccel-devel kf6-kdbusaddons-devel plasma-devel layer-shell-qt-devel qt6-qtbase-devel qt6-qtdeclarative-devel
```

#### Ubuntu / Debian / KDE Neon
```bash
sudo apt install extra-cmake-modules libkf6globalaccel-dev libkf6dbusaddons-dev libplasma-dev liblayershellqtinterface-dev qt6-base-dev qt6-declarative-dev
# Note: On some Debian/Ubuntu/Neon versions, libplasma-dev may be packaged as libplasma6-dev instead.
```

### Installation

#### 1. Quick Installation (User-Local)
You can build and install Scribbleway user-locally under `~/.local/` using the automated installation script:

```bash
chmod +x install.sh
./install.sh
```

To run the installation script unattended (automatically configuring shell profiles without prompts):
```bash
./install.sh --yes
```

#### 2. Manual Building & Installation
To build and install the project manually:

```bash
# 1. Configure the build
cmake -B build -S . -DCMAKE_INSTALL_PREFIX="$HOME/.local" -DCMAKE_BUILD_TYPE=Release

# 2. Compile the binaries and plugins
cmake --build build -j$(nproc)

# 3. Install
cmake --install build
```

*Note: For manual installations, you may need to export the following environment variables in your `~/.bashrc` or `~/.profile` for the QML plugin to load correctly:*
```bash
export QML2_IMPORT_PATH="$HOME/.local/lib/qml:$HOME/.local/lib/x86_64-linux-gnu/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
export QT_PLUGIN_PATH="$HOME/.local/lib/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
```

#### 3. Uninstallation
To completely revert a user-local installation and clean up desktop shortcuts/autostart files:
```bash
chmod +x uninstall.sh
./uninstall.sh
```

To run the uninstallation unattended:
```bash
./uninstall.sh --yes
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
---

## Default Keyboard Shortcuts

| Tool / Action | Global Hotkey |
| :--- | :--- |
| **Draw Freehand** | `Meta` + `Shift` + `F` |
| **Draw Arrow** | `Meta` + `Shift` + `A` |
| **Draw Rectangle** | `Meta` + `Shift` + `V` |
| **Draw Ellipse** | `Meta` + `Shift` + `E` |
| **Draw Line** | `Meta` + `Shift` + `L` |
| **Draw Text** | `Meta` + `Shift` + `T` |
| **Toggle Select Mode** | `Meta` + `Shift` + `S` |
| **Undo Last Shape** | `Meta` + `Ctrl` + `Z` |
| **Clear All Shapes** | `Meta` + `Ctrl` + `Delete` |

*(Note: Shortcuts can be rebound dynamically in the Plasma applet settings).*
