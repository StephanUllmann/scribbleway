# Design Spec: Glow Effect for Drawn Shapes

*   **Date:** 2026-07-12
*   **Status:** Draft
*   **Author:** Antigravity (Advanced Agentic Coding)

## 1. Goal
Implement a hardware-accelerated, customizable glow effect for all drawn shapes in Scribbleway. The glow should look like a soft neon/light outline extending outward from the shape, while leaving the interior foreground of the shape sharp and unblurred.

## 2. Requirements & Constraints
*   **CSS-like Pixels:** The glow blur radius must be configured and stored in pixels, ranging from `0px` (disabled) to `15px` (maximum).
*   **Sharp Foreground:** The original shape's stroke and fill must not be blurred or softened; the glow must render *behind* the shape's sharp foreground.
*   **Applet Controls:** The Plasma applet must display a dedicated "Glow" slider with a range of `0px` to `15px` and a step size of `1px`.
*   **Sensible Default:** New shapes drawn within Scribbleway must default to `3px` glow.
*   **Excalidraw Compatibility:**
    *   Pasting from Excalidraw must default to `0px` (no glow) to preserve the original visual style of pasted shapes.
    *   Copying to Excalidraw must include the property in the JSON, which Excalidraw ignores safely, preventing any clipboard crashes or structural failures.

---

## 3. Architecture & Technical Design

### 3.1 QML Rendering with `MultiEffect`
We will use the hardware-accelerated `MultiEffect` from `QtQuick.Effects` (available in Qt 6.5+ / already present in the workspace environment). 

Inside each shape file (`RectangleShape.qml`, `EllipseShape.qml`, `LineShape.qml`, `ArrowShape.qml`, `FreehandShape.qml`), the visual elements will be wrapped in a container item named `shapeContent`. The `MultiEffect` will sit behind this container in the QML declaration order and use `shapeContent` as its source:

```qml
import QtQuick
import QtQuick.Effects

Item {
    id: root
    
    // ... coordinates and shape properties ...
    property int modelGlow: model.glow !== undefined ? model.glow : 0

    MultiEffect {
        id: glowEffect
        anchors.fill: shapeContent
        source: shapeContent
        visible: root.modelGlow > 0
        
        blurEnabled: true
        blurMax: 15                  // Matches maximum glow limit
        blur: root.modelGlow / 15.0  // Normalized blur radius (0.0 to 1.0)
    }

    Item {
        id: shapeContent
        anchors.fill: parent
        
        // Original drawing elements (Rectangle, RoughStroke, ShapePath, etc.)
    }
}
```

*Note: Since QML renders components in declaration order, placing `MultiEffect` before `shapeContent` guarantees the glow stays behind the sharp foreground.*

### 3.2 Data Model Changes
1.  **ShapesModel Roles (`src/overlay/shapesmodel.h` / `src/overlay/shapesmodel.cpp`):**
    *   Add `GlowRole` enum element mapping to the `"glow"` property name.
2.  **Default Value Configuration (`src/overlay/overlaycontroller.h` / `src/overlay/overlaycontroller.cpp`):**
    *   Introduce `m_defaultGlow` state variable initialized to `3`.
    *   Update `setDefaultGlow(int glow)` and `defaultGlow()` methods.
    *   Ensure newly created shapes default to `m_defaultGlow` unless overridden.

### 3.3 DBus & Applet Plugin Synchronization
1.  **AppletBackend Class (`src/applet-plugin/appletbackend.h` / `src/applet-plugin/appletbackend.cpp`):**
    *   Add `selectedGlow` property (`Q_PROPERTY(int selectedGlow READ selectedGlow NOTIFY selectionChanged)`).
    *   Add `setGlow(int glow)` Q_INVOKABLE slot.
    *   Retrieve the glow level from the selection state map inside `onSelectionChanged`.

### 3.4 Clipboard & Compatibility
1.  **Serialization (`OverlayController::convertToExcalidraw`):**
    *   Serialize `glow` into the exported JSON object as `"glow"`. Excalidraw ignores this key, keeping the clipboard interchange robust.
2.  **Deserialization (`OverlayController::convertFromExcalidraw`):**
    *   When deserializing pasted JSON:
        ```cpp
        shape.insert(QStringLiteral("glow"), elem.value(QStringLiteral("glow")).toInt(0));
        ```
        If `"glow"` is absent (e.g., pasting from raw Excalidraw), it defaults to `0` (no glow).

---

## 4. UI Design (Plasma Applet)
We will insert a new row in `FullRepresentation.qml` right under the "Opacity" slider:

```qml
// Row: Glow
RowLayout {
    Layout.fillWidth: true
    
    PlasmaComponents.Label {
        text: "Glow:"
        width: Kirigami.Units.gridUnit * 3
    }
    
    PlasmaComponents.Slider {
        Layout.fillWidth: true
        from: 0
        to: 15
        stepSize: 1
        value: root.backend.hasSelection ? root.backend.selectedGlow : 3
        onMoved: {
            root.backend.setGlow(value)
        }
    }

    PlasmaComponents.Label {
        text: Math.round(root.backend.hasSelection ? root.backend.selectedGlow : 3) + "px"
    }
}
```

---

## 5. Verification Plan
*   **Unit Tests:** Add tests to `tests/shapesmodeltest.cpp` verifying:
    *   Default glow for new shapes is `3`.
    *   Updating glow property via controller works.
    *   Pasted shapes from Excalidraw format default to `0` glow.
*   **Manual Verification:**
    *   Verify the glow slider renders in the Plasma applet.
    *   Verify shapes drawn on screen render with a soft glowing background when the slider is set > 0.
    *   Verify copying shapes from Scribbleway and pasting to Excalidraw works.
    *   Verify pasting shapes from Excalidraw into Scribbleway defaults to no glow.
