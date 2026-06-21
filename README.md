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

### Dependencies

Ensure you have the following development libraries installed:
- **Qt 6** (Core, Gui, Qml, Quick, DBus)
- **KDE Frameworks 6** (ECM, GlobalAccel, DBusAddons)
- **Plasma 6** (Plasma library headers)
- **LayerShellQt**

### Building from Source

```bash
# 1. Create and enter build directory
mkdir build && cd build

# 2. Configure the project
cmake ..

# 3. Compile the binaries and plugins
make
```

### Running the Daemon

Launch the overlay helper daemon manually from your build tree:
```bash
./src/overlay/scribbleway-overlay
```

To run unit tests:
```bash
ctest
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
