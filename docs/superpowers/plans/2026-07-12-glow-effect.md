# Glow Effect Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a hardware-accelerated, customizable glow effect for all drawn shapes in Scribbleway with a slider control in the applet, keeping Excalidraw compatibility intact.

**Architecture:** We will use `MultiEffect` from `QtQuick.Effects` to render a blurred shadow of the shape behind the sharp foreground. We will store `glow` (0-15px) in the data model and synchronize it between the daemon and applet over DBus.

**Tech Stack:** Qt 6.6+, C++20, QML, D-Bus, Kirigami, QtQuick.Effects.

## Global Constraints
*   **CSS-like Pixels:** Glow radius must range from `0` to `15` px.
*   **Sharp Foreground:** The original shape's stroke and fill must not be blurred.
*   **Default Glow:** New shapes created in Scribbleway must default to `3` px.
*   **Excalidraw Compatibility:** Pasted shapes from Excalidraw must default to `0` px.

---

## File Structure & Decomposition
*   `src/overlay/shapesmodel.h` / `shapesmodel.cpp`: Add `GlowRole` mapping to the QML `"glow"` property.
*   `src/overlay/overlaycontroller.h` / `overlaycontroller.cpp`: Manage default glow level and map `glow` in Excalidraw clipboard serialization.
*   `src/applet-plugin/appletbackend.h` / `appletbackend.cpp`: Expose selection glow property and update slot to the Plasma applet.
*   `src/overlay/qml/shapes/BaseShape.qml`: Expose `modelGlow` property.
*   `src/overlay/qml/main.qml`: Default new shapes to `controller.defaultGlow`.
*   `src/overlay/qml/shapes/{RectangleShape,EllipseShape,LineShape,ArrowShape,FreehandShape}.qml`: Wrap shape visual components inside `shapeContent` and add a `MultiEffect` sibling behind it.
*   `applet/contents/ui/FullRepresentation.qml`: Add "Glow" slider.
*   `debian/control`: Add package runtime dependency `qml6-module-qtquick-effects`.
*   `tests/shapesmodeltest.cpp`: Add unit tests for default values, updates, and Excalidraw paste compatibility.

---

### Task 1: Update Data Model & Serialization (Daemon Side)

**Files:**
*   Modify: `src/overlay/shapesmodel.h`
*   Modify: `src/overlay/shapesmodel.cpp`
*   Modify: `src/overlay/overlaycontroller.h`
*   Modify: `src/overlay/overlaycontroller.cpp`
*   Test: `tests/shapesmodeltest.cpp` (Will be compiled in this task)

**Interfaces:**
*   Consumes: None.
*   Produces: `ShapesModel::GlowRole`, `OverlayController::defaultGlow()`, `OverlayController::setDefaultGlow()`, and shape serialization/deserialization for `glow`.

- [ ] **Step 1: Add GlowRole enum element**

Modify `src/overlay/shapesmodel.h` around line 35 to declare `GlowRole`:
```cpp
        RoughnessRole,
        SeedRole,
        GlowRole
    };
```

- [ ] **Step 2: Map GlowRole in ShapesModel**

Modify `src/overlay/shapesmodel.cpp` to return the value of `glow` and register its string role name:
Around line 65 in `data()`:
```cpp
        case SeedRole: return shape.value(QStringLiteral("seed"));
        case GlowRole: return shape.value(QStringLiteral("glow"));
        default: return QVariant();
```
Around line 94 in `roleNames()`:
```cpp
    roles[SeedRole] = "seed";
    roles[GlowRole] = "glow";
    return roles;
```
Around line 205 in `updateShape()`:
```cpp
                else if (it.key() == QStringLiteral("seed")) changedRoles << SeedRole;
                else if (it.key() == QStringLiteral("glow")) changedRoles << GlowRole;
```

- [ ] **Step 3: Declare defaultGlow property & functions in OverlayController**

Modify `src/overlay/overlaycontroller.h` to declare Q_PROPERTY, getter, setter, notifier, and backing member:
Around line 44:
```cpp
    Q_PROPERTY(int defaultRoughness READ defaultRoughness WRITE setDefaultRoughness NOTIFY defaultRoughnessChanged)
    Q_PROPERTY(int defaultGlow READ defaultGlow WRITE setDefaultGlow NOTIFY defaultGlowChanged)
```
Around line 80:
```cpp
    int defaultRoughness() const;
    void setDefaultRoughness(int roughness);
    int defaultGlow() const;
    void setDefaultGlow(int glow);
    void setDefaultBorderRadius(int radius);
```
Around line 144:
```cpp
    void defaultRoughnessChanged();
    void defaultGlowChanged();
```
Around line 182:
```cpp
    int m_defaultRoughness = 1;
    int m_defaultGlow = 3;
```

- [ ] **Step 4: Implement defaultGlow functions and Clipboard Serialization in OverlayController**

Modify `src/overlay/overlaycontroller.cpp`:
Around line 246 to implement getter and setter:
```cpp
int OverlayController::defaultGlow() const
{
    return m_defaultGlow;
}

void OverlayController::setDefaultGlow(int glow)
{
    if (m_defaultGlow != glow) {
        m_defaultGlow = glow;
        Q_EMIT defaultGlowChanged();
    }
}
```
Around line 310 inside the deserialize method (e.g. from JSON state):
```cpp
            if (shape.contains(QStringLiteral("roughness"))) {
                m_defaultRoughness = shape[QStringLiteral("roughness")].toInt();
            }
            if (shape.contains(QStringLiteral("glow"))) {
                m_defaultGlow = shape[QStringLiteral("glow")].toInt();
            }
            Q_EMIT defaultColorChanged();
```
Around line 372 in `getSelectionState()`:
```cpp
        state[QStringLiteral("roughness")] = shape.value(QStringLiteral("roughness"), m_defaultRoughness).toInt();
        state[QStringLiteral("glow")] = shape.value(QStringLiteral("glow"), m_defaultGlow).toInt();
        state[QStringLiteral("seed")] = shape.value(QStringLiteral("seed"), 123456).toInt();
```
And around line 386 in the default state branch of `getSelectionState()`:
```cpp
        state[QStringLiteral("roughness")] = m_defaultRoughness;
        state[QStringLiteral("glow")] = m_defaultGlow;
        state[QStringLiteral("locked")] = false;
```
Around line 414 in `updateProperties()`:
```cpp
    if (demarshalled.contains(QStringLiteral("roughness"))) {
        setDefaultRoughness(demarshalled[QStringLiteral("roughness")].toInt());
    }
    if (demarshalled.contains(QStringLiteral("glow"))) {
        setDefaultGlow(demarshalled[QStringLiteral("glow")].toInt());
    }
```
Around line 1104 in `convertToExcalidraw()`:
```cpp
    elem.insert(QStringLiteral("roughness"), shape.value(QStringLiteral("roughness"), 1).toInt());
    elem.insert(QStringLiteral("glow"), shape.value(QStringLiteral("glow"), 0).toInt()); // Ignore/preserve
```
Around line 1232 in `convertFromExcalidraw()`:
```cpp
    shape.insert(QStringLiteral("roughness"), elem.value(QStringLiteral("roughness")).toInt(1));
    shape.insert(QStringLiteral("glow"), elem.value(QStringLiteral("glow")).toInt(0)); // Default to 0px
```

- [ ] **Step 5: Verify build compiles**

Run: `cmake -B build -S . && cmake --build build --target scribbleway-overlay`
Expected: Compile succeeds with no errors.

- [ ] **Step 6: Commit**

```bash
git add src/overlay/shapesmodel.h src/overlay/shapesmodel.cpp src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp
git commit -m "feat: add glow property to ShapesModel and OverlayController"
```

---

### Task 2: Update Applet Backend (DBus Plugin)

**Files:**
*   Modify: `src/applet-plugin/appletbackend.h`
*   Modify: `src/applet-plugin/appletbackend.cpp`

**Interfaces:**
*   Consumes: `OverlayController::updateProperties`, selection state maps from Task 1.
*   Produces: `AppletBackend::selectedGlow` property, `AppletBackend::setGlow(int glow)` slot.

- [ ] **Step 1: Declare selectedGlow property & functions**

Modify `src/applet-plugin/appletbackend.h`:
Around line 25 to add property:
```cpp
    Q_PROPERTY(int selectedBorderRadius READ selectedBorderRadius NOTIFY selectionChanged)
    Q_PROPERTY(int selectedGlow READ selectedGlow NOTIFY selectionChanged)
```
Around line 46 to add getter:
```cpp
    int selectedBorderRadius() const;
    int selectedGlow() const;
```
Around line 62 to add slot:
```cpp
    Q_INVOKABLE void setBorderRadius(int radius);
    Q_INVOKABLE void setGlow(int glow);
```
Around line 109 to add member variable:
```cpp
    int m_selectedBorderRadius = 8;
    int m_selectedGlow = 3;
```

- [ ] **Step 2: Implement selectedGlow functions and update logic**

Modify `src/applet-plugin/appletbackend.cpp`:
Around line 75 to implement getter:
```cpp
int AppletBackend::selectedGlow() const
{
    return m_selectedGlow;
}
```
Around line 149 to implement setter forwarding property update via D-Bus:
```cpp
void AppletBackend::setGlow(int glow)
{
    sendDBus(QStringLiteral("updateProperties"), { QVariantMap{{QStringLiteral("glow"), glow}} });
}
```
Around line 335 in `onSelectionChanged` to parse glow level from selection state:
```cpp
    m_selectedRoughness = state.value(QStringLiteral("roughness")).toInt();
    m_selectedGlow = state.value(QStringLiteral("glow")).toInt();
    m_selectedLocked = state.value(QStringLiteral("locked")).toBool();
```

- [ ] **Step 3: Verify plugin compiles**

Run: `cmake --build build --target scribblewaybackend`
Expected: Compile succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add src/applet-plugin/appletbackend.h src/applet-plugin/appletbackend.cpp
git commit -m "feat: expose selectedGlow property and setGlow slot in AppletBackend"
```

---

### Task 3: QML Shapes Glow Rendering

**Files:**
*   Modify: `src/overlay/qml/shapes/BaseShape.qml`
*   Modify: `src/overlay/qml/main.qml`
*   Modify: `src/overlay/qml/shapes/RectangleShape.qml`
*   Modify: `src/overlay/qml/shapes/EllipseShape.qml`
*   Modify: `src/overlay/qml/shapes/LineShape.qml`
*   Modify: `src/overlay/qml/shapes/ArrowShape.qml`
*   Modify: `src/overlay/qml/shapes/FreehandShape.qml`
*   Modify: `src/overlay/qml/shapes/TextShape.qml`

**Interfaces:**
*   Consumes: `model.glow` role, `controller.defaultGlow` from Task 1.
*   Produces: Glow rendering effect in Overlay UI for all shape types.

- [ ] **Step 1: Expose modelGlow in BaseShape**

Modify `src/overlay/qml/shapes/BaseShape.qml` around line 18:
```qml
    property int modelRoughness: model.roughness !== undefined ? model.roughness : 0
    property int modelGlow: model.glow !== undefined ? model.glow : 0
    property int modelSeed: model.seed !== undefined ? model.seed : 123456
```

- [ ] **Step 2: Assign default glow to new shapes**

Modify `src/overlay/qml/main.qml` around line 127 in `finalizeShape()`:
```javascript
            "locked": false,
            "roughness": controller.defaultRoughness,
            "glow": controller.defaultGlow,
            "seed": Math.floor(Math.random() * 1000000) + 1
```

- [ ] **Step 3: Wrap RectangleShape visuals and add MultiEffect**

Modify `src/overlay/qml/shapes/RectangleShape.qml`:
Add `import QtQuick.Effects` at the top. Wrap the existing `Rectangle` and `RoughStroke` in a container `Item` named `shapeContent`, and add `MultiEffect` behind it:
```qml
import QtQuick
import QtQuick.Effects
import "RoughPathGenerator.js" as RoughPathGenerator

BaseShape {
    id: root

    mode: "rect"
    shapeIndex: index
    shapeX: model.x
    shapeY: model.y
    shapeWidth: model.width
    shapeHeight: model.height

    onRectGeometryChanged: (nx, ny, nw, nh) => {
        controller.updateShape(index, { "x": nx, "y": ny, "width": nw, "height": nh });
    }

    MultiEffect {
        id: glowEffect
        anchors.fill: shapeContent
        source: shapeContent
        visible: root.modelGlow > 0
        
        blurEnabled: true
        blurMax: 15
        blur: root.modelGlow / 15.0
    }

    Item {
        id: shapeContent
        anchors.fill: parent

        Rectangle {
            x: root.shapeX
            y: root.shapeY
            width: root.shapeWidth
            height: root.shapeHeight
            opacity: root.modelOpacity

            border.color: root.modelColor
            border.width: root.modelRoughness === 0 ? root.modelStrokeWidth : 0
            color: {
                let c = Qt.color(root.modelColor);
                return Qt.rgba(c.r, c.g, c.b, 0.12);
            }
            radius: typeof borderRadius !== "undefined" ? borderRadius : 0
        }

        RoughStroke {
            anchors.fill: parent
            strokes: root.modelRoughness > 0
                ? RoughPathGenerator.getSketchyRectangle(
                    root.shapeX, root.shapeY, root.shapeWidth, root.shapeHeight,
                    root.modelRoughness, root.modelSeed,
                    typeof borderRadius !== "undefined" ? borderRadius : 0)
                : []
            strokeColor: root.modelColor
            strokeWidth: root.modelStrokeWidth
            strokeOpacity: root.modelOpacity
        }
    }
}
```

- [ ] **Step 4: Wrap EllipseShape visuals and add MultiEffect**

Modify `src/overlay/qml/shapes/EllipseShape.qml`:
Add `import QtQuick.Effects` at the top. Wrap the existing `Shape` and `RoughStroke` in a container `Item` named `shapeContent`, and add `MultiEffect` behind it:
```qml
import QtQuick
import QtQuick.Shapes
import QtQuick.Effects
import "RoughPathGenerator.js" as RoughPathGenerator

BaseShape {
    id: root

    mode: "rect"
    shapeIndex: index
    shapeX: model.x
    shapeY: model.y
    shapeWidth: model.width
    shapeHeight: model.height

    onRectGeometryChanged: (nx, ny, nw, nh) => {
        controller.updateShape(index, { "x": nx, "y": ny, "width": nw, "height": nh });
    }

    MultiEffect {
        id: glowEffect
        anchors.fill: shapeContent
        source: shapeContent
        visible: root.modelGlow > 0
        
        blurEnabled: true
        blurMax: 15
        blur: root.modelGlow / 15.0
    }

    Item {
        id: shapeContent
        anchors.fill: parent

        Shape {
            anchors.fill: parent
            opacity: root.modelOpacity

            ShapePath {
                strokeColor: root.modelRoughness === 0 ? root.modelColor : "transparent"
                strokeWidth: root.modelStrokeWidth
                fillColor: {
                    let c = Qt.color(root.modelColor);
                    return Qt.rgba(c.r, c.g, c.b, 0.12);
                }

                startX: root.shapeX + root.shapeWidth / 2
                startY: root.shapeY

                PathArc {
                    x: root.shapeX + root.shapeWidth / 2
                    y: root.shapeY + root.shapeHeight
                    radiusX: root.shapeWidth / 2
                    radiusY: root.shapeHeight / 2
                    useLargeArc: false
                    direction: PathArc.Clockwise
                }

                PathArc {
                    x: root.shapeX + root.shapeWidth / 2
                    y: root.shapeY
                    radiusX: root.shapeWidth / 2
                    radiusY: root.shapeHeight / 2
                    useLargeArc: false
                    direction: PathArc.Clockwise
                }
            }
        }

        RoughStroke {
            anchors.fill: parent
            strokes: root.modelRoughness > 0
                ? RoughPathGenerator.getSketchyEllipse(
                    root.shapeX, root.shapeY, root.shapeWidth, root.shapeHeight,
                    root.modelRoughness, root.modelSeed)
                : []
            strokeColor: root.modelColor
            strokeWidth: root.modelStrokeWidth
            strokeOpacity: root.modelOpacity
        }
    }
}
```

- [ ] **Step 5: Wrap LineShape visuals and add MultiEffect**

Modify `src/overlay/qml/shapes/LineShape.qml`:
Add `import QtQuick.Effects` at the top. Wrap the existing `Shape` and `RoughStroke` in a container `Item` named `shapeContent`, and add `MultiEffect` behind it:
```qml
import QtQuick
import QtQuick.Shapes
import QtQuick.Effects
import "RoughPathGenerator.js" as RoughPathGenerator

BaseShape {
    id: root

    mode: "line"
    shapeIndex: index
    shapeFromX: model.fromX
    shapeFromY: model.fromY
    shapeToX: model.toX
    shapeToY: model.toY

    onLineGeometryChanged: (nfx, nfy, ntx, nty) => {
        controller.updateShape(index, { "fromX": nfx, "fromY": nfy, "toX": ntx, "toY": nty });
    }

    MultiEffect {
        id: glowEffect
        anchors.fill: shapeContent
        source: shapeContent
        visible: root.modelGlow > 0
        
        blurEnabled: true
        blurMax: 15
        blur: root.modelGlow / 15.0
    }

    Item {
        id: shapeContent
        anchors.fill: parent

        Shape {
            anchors.fill: parent
            opacity: root.modelOpacity
            visible: root.modelRoughness === 0

            ShapePath {
                strokeColor: root.modelColor
                strokeWidth: root.modelStrokeWidth
                fillColor: "transparent"

                startX: root.shapeFromX
                startY: root.shapeFromY

                PathLine {
                    x: root.shapeToX
                    y: root.shapeToY
                }
            }
        }

        RoughStroke {
            anchors.fill: parent
            strokes: root.modelRoughness > 0
                ? RoughPathGenerator.getSketchyLine(
                    root.shapeFromX, root.shapeFromY, root.shapeToX, root.shapeToY,
                    root.modelRoughness, root.modelSeed)
                : []
            strokeColor: root.modelColor
            strokeWidth: root.modelStrokeWidth
            strokeOpacity: root.modelOpacity
        }
    }
}
```

- [ ] **Step 6: Wrap ArrowShape visuals and add MultiEffect**

Modify `src/overlay/qml/shapes/ArrowShape.qml`:
Add `import QtQuick.Effects` at the top. Wrap the existing `Shape` and `RoughStroke` in a container `Item` named `shapeContent`, and add `MultiEffect` behind it:
```qml
import QtQuick
import QtQuick.Shapes
import QtQuick.Effects
import "RoughPathGenerator.js" as RoughPathGenerator

BaseShape {
    id: root

    mode: "line"
    shapeIndex: index
    shapeFromX: model.fromX
    shapeFromY: model.fromY
    shapeToX: model.toX
    shapeToY: model.toY

    onLineGeometryChanged: (nfx, nfy, ntx, nty) => {
        controller.updateShape(index, { "fromX": nfx, "fromY": nfy, "toX": ntx, "toY": nty });
    }

    readonly property real lineAngle: Math.atan2(shapeToY - shapeFromY, shapeToX - shapeFromX)
    readonly property real arrowLength: 10 + root.modelStrokeWidth * 1.5
    readonly property real arrowHalfAngle: Math.PI / 6
    readonly property real arrowLeftX: shapeToX - arrowLength * Math.cos(lineAngle - arrowHalfAngle)
    readonly property real arrowLeftY: shapeToY - arrowLength * Math.sin(lineAngle - arrowHalfAngle)
    readonly property real arrowRightX: shapeToX - arrowLength * Math.cos(lineAngle + arrowHalfAngle)
    readonly property real arrowRightY: shapeToY - arrowLength * Math.sin(lineAngle + arrowHalfAngle)
    readonly property real lineLength: Math.sqrt(Math.pow(shapeToX - shapeFromX, 2) + Math.pow(shapeToY - shapeFromY, 2))
    readonly property real stemLength: Math.max(0, lineLength - arrowLength * Math.cos(arrowHalfAngle))
    readonly property real arrowBaseX: shapeFromX + stemLength * Math.cos(lineAngle)
    readonly property real arrowBaseY: shapeFromY + stemLength * Math.sin(lineAngle)

    MultiEffect {
        id: glowEffect
        anchors.fill: shapeContent
        source: shapeContent
        visible: root.modelGlow > 0
        
        blurEnabled: true
        blurMax: 15
        blur: root.modelGlow / 15.0
    }

    Item {
        id: shapeContent
        anchors.fill: parent

        Shape {
            anchors.fill: parent
            opacity: root.modelOpacity
            preferredRendererType: Shape.CurveRenderer
            visible: root.modelRoughness === 0

            ShapePath {
                strokeColor: root.modelColor
                strokeWidth: root.modelStrokeWidth
                fillColor: "transparent"

                startX: root.shapeFromX
                startY: root.shapeFromY

                PathLine {
                    x: root.arrowBaseX
                    y: root.arrowBaseY
                }
            }

            ShapePath {
                strokeColor: "transparent"
                fillColor: root.modelColor

                startX: root.shapeToX
                startY: root.shapeToY

                PathLine {
                    x: root.arrowLeftX
                    y: root.arrowLeftY
                }

                PathLine {
                    x: root.arrowRightX
                    y: root.arrowRightY
                }

                PathLine {
                    x: root.shapeToX
                    y: root.shapeToY
                }
            }
        }

        RoughStroke {
            anchors.fill: parent
            strokes: root.modelRoughness > 0
                ? RoughPathGenerator.getSketchyArrow(
                    root.shapeFromX, root.shapeFromY, root.shapeToX, root.shapeToY,
                    root.modelRoughness, root.modelSeed, root.arrowLength)
                : []
            strokeColor: root.modelColor
            strokeWidth: root.modelStrokeWidth
            strokeOpacity: root.modelOpacity
        }
    }
}
```

- [ ] **Step 7: Wrap FreehandShape visuals and add MultiEffect**

Modify `src/overlay/qml/shapes/FreehandShape.qml`:
Add `import QtQuick.Effects` at the top. Wrap the existing `Shape` and `RoughStroke` in a container `Item` named `shapeContent`, and add `MultiEffect` behind it:
```qml
import QtQuick
import QtQuick.Shapes
import QtQuick.Effects
import "RoughPathGenerator.js" as RoughPathGenerator

BaseShape {
    id: root

    mode: "none"
    shapeIndex: index

    readonly property var points: model.points || []

    MultiEffect {
        id: glowEffect
        anchors.fill: shapeContent
        source: shapeContent
        visible: root.modelGlow > 0
        
        blurEnabled: true
        blurMax: 15
        blur: root.modelGlow / 15.0
    }

    Item {
        id: shapeContent
        anchors.fill: parent

        Shape {
            anchors.fill: parent
            opacity: root.modelOpacity
            visible: root.modelRoughness === 0

            ShapePath {
                strokeColor: root.modelColor
                strokeWidth: root.modelStrokeWidth
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                startX: root.points.length > 0 ? root.points[0].x : 0
                startY: root.points.length > 0 ? root.points[0].y : 0

                Repeater {
                    model: root.points.length > 1 ? root.points.slice(1) : 0
                    PathLine {
                        x: modelData.x
                        y: modelData.y
                    }
                }
            }
        }

        RoughStroke {
            anchors.fill: parent
            strokes: root.modelRoughness > 0
                ? RoughPathGenerator.getSketchyFreehand(root.points, root.modelRoughness, root.modelSeed)
                : []
            strokeColor: root.modelColor
            strokeWidth: root.modelStrokeWidth
            strokeOpacity: root.modelOpacity
        }
    }
}
```

- [ ] **Step 8: Wrap TextShape visuals and add MultiEffect**

Modify `src/overlay/qml/shapes/TextShape.qml`:
Add `import QtQuick.Effects` at the top. Wrap the existing `Text` in a container `Item` named `shapeContent`, and add `MultiEffect` behind it:
```qml
import QtQuick
import QtQuick.Effects

BaseShape {
    id: root
    
    mode: "none"
    shapeIndex: index
    shapeX: model.x
    shapeY: model.y
    shapeWidth: model.width
    shapeHeight: model.height

    onRectGeometryChanged: (nx, ny, nw, nh) => {
        controller.updateShape(index, { "x": nx, "y": ny });
    }

    MultiEffect {
        id: glowEffect
        anchors.fill: shapeContent
        source: shapeContent
        visible: root.modelGlow > 0
        
        blurEnabled: true
        blurMax: 15
        blur: root.modelGlow / 15.0
    }

    Item {
        id: shapeContent
        anchors.fill: parent

        Text {
            id: textLabel
            x: root.shapeX + 5
            y: root.shapeY + 5
            text: model.text || ""
            color: root.modelColor
            opacity: root.modelOpacity
            font.family: typeof fontFamily !== "undefined" ? fontFamily : controller.defaultFontFamily
            font.pixelSize: typeof fontSize !== "undefined" ? fontSize : 20

            onImplicitWidthChanged: syncSize()
            onImplicitHeightChanged: syncSize()
            
            Component.onCompleted: syncSize()

            function syncSize() {
                controller.updateShape(index, { 
                    "width": Math.max(50, implicitWidth + 10), 
                    "height": Math.max(20, implicitHeight + 10) 
                });
            }
        }
    }

    onDoubleClicked: {
        if (typeof canvasWindow !== "undefined") {
            canvasWindow.startTextEditing(root.shapeIndex);
        }
    }
}
```

- [ ] **Step 9: Commit**

```bash
git add src/overlay/qml/main.qml src/overlay/qml/shapes/BaseShape.qml src/overlay/qml/shapes/RectangleShape.qml src/overlay/qml/shapes/EllipseShape.qml src/overlay/qml/shapes/LineShape.qml src/overlay/qml/shapes/ArrowShape.qml src/overlay/qml/shapes/FreehandShape.qml src/overlay/qml/shapes/TextShape.qml
git commit -m "feat: implement MultiEffect-based glow rendering in QML shapes"
```

---

### Task 4: Applet UI Integration

**Files:**
*   Modify: `applet/contents/ui/FullRepresentation.qml`

**Interfaces:**
*   Consumes: `root.backend.selectedGlow` property, `root.backend.setGlow()` slot from Task 2.
*   Produces: UI controls (slider) for setting shape glow level in the Plasmoid settings dialog.

- [ ] **Step 1: Add Glow Slider Row**

Modify `applet/contents/ui/FullRepresentation.qml` to insert the Glow slider row right below the Opacity row (around line 343):
```qml
        // Row 4.5: Glow
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

- [ ] **Step 2: Verify applet files build**

Run: `cmake --build build --target rebuild_debian_package`
Expected: Debian package build script runs and completes successfully.

- [ ] **Step 3: Commit**

```bash
git add applet/contents/ui/FullRepresentation.qml
git commit -m "feat: add glow slider control to Plasma applet FullRepresentation"
```

---

### Task 5: Packaging & Verification (TDD)

**Files:**
*   Modify: `debian/control`
*   Modify: `tests/shapesmodeltest.cpp`

**Interfaces:**
*   Consumes: All components from Tasks 1-4.
*   Produces: Verified packages and tests ensuring correct default glow and clipboard behavior.

- [ ] **Step 1: Update package runtime dependency**

Modify `debian/control` around line 24 to append `qml6-module-qtquick-effects` to runtime dependencies:
```
Depends: ${shlibs:Depends},
         ${misc:Depends},
         qml6-module-qtquick-layouts,
         qml6-module-qtquick-controls,
         qml6-module-qtquick-templates,
         qml6-module-qtquick-shapes,
         qml6-module-qtquick-effects
```

- [ ] **Step 2: Add testGlow test declaration**

Modify `tests/shapesmodeltest.cpp` around line 40 to declare `testGlow`:
```cpp
    void testRoughness();
    void testGlow();
    void testExcalidrawPasteCompatibility();
```

- [ ] **Step 3: Implement testGlow unit test**

Modify `tests/shapesmodeltest.cpp` (at the bottom, just before `testExcalidrawPasteCompatibility` implementation):
```cpp
void ShapesModelTest::testGlow()
{
    OverlayController controller;

    // 1. Check initial default glow is 3
    QCOMPARE(controller.defaultGlow(), 3);

    // 2. Update default glow
    QVariantMap updateProps;
    updateProps[QStringLiteral("glow")] = 10;
    controller.updateProperties(updateProps);

    QCOMPARE(controller.defaultGlow(), 10);

    // 3. Create a shape and ensure it gets default glow
    QVariantMap shape;
    shape[QStringLiteral("type")] = QStringLiteral("rectangle");
    shape[QStringLiteral("glow")] = controller.defaultGlow();
    controller.addShape(shape);

    QCOMPARE(controller.selectedIndex(), 0);
    QCOMPARE(controller.getSelectionState()[QStringLiteral("glow")].toInt(), 10);
}
```

- [ ] **Step 4: Update Excalidraw paste compatibility test**

Modify `tests/shapesmodeltest.cpp` inside `testExcalidrawPasteCompatibility()` to verify pasted elements default to `0` glow.
Around line 614:
```cpp
    rectObj.insert(QStringLiteral("version"), 1);
    rectObj.insert(QStringLiteral("roughness"), 1);
    // glow not inserted, simulates genuine Excalidraw element
```
At the bottom of `testExcalidrawPasteCompatibility()` (around line 730):
```cpp
    // Verify pasted shape has glow defaulted to 0
    QCOMPARE(controller.shapesModel()->shapes().first().value(QStringLiteral("glow")).toInt(), 0);
```

- [ ] **Step 5: Run tests and verify they pass**

Run: `cmake --build build --target test`
Expected: All tests pass, including the new glow tests.
Output:
```
1/1 Test #1: shapesmodeltest ..................   Passed
```

- [ ] **Step 6: Commit and Rebuild Debian package**

Run:
```bash
git add debian/control tests/shapesmodeltest.cpp
git commit -m "test: add unit tests for glow effect and update package dependencies"
cmake --build build --target rebuild_debian_package
```
Expected: Build finishes with a new `.deb` file.
