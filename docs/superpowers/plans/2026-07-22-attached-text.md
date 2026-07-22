# Attached Text Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Excalidraw-style attached text for rectangles, ellipses, lines, and arrows.

**Architecture:** Store attached text as container-owned binding data in the existing `QVariantMap` shape model. Render/edit attached text from the container shape, not as an independently selectable `text` row. Clipboard conversion synthesizes real Excalidraw bound text elements for rectangle/ellipse/arrow and a metadata-backed fallback text element for lines.

**Tech Stack:** C++20, Qt 6/QML, `QAbstractListModel`, `QVariantMap`, Qt Test, Excalidraw clipboard JSON.

## Global Constraints

- No new dependencies.
- Attached text is not selectable individually.
- Attached text moves with its containing shape because position is derived from container geometry.
- Attached text follows the containing shape's `color`, `opacity`, `fontFamily`, and `fontSize`.
- Empty attached text commit removes only the attachment, not the containing shape.
- Rectangle, ellipse, and arrow attached text round-trip with Excalidraw bound text JSON.
- Line attached text is supported locally and exports as a centered fallback text element with Scribbleway `customData`.
- Text wraps inside the available centered area; overflow remains visible and never resizes the container.
- After each successful full build, rebuild the `.deb` package. The existing CMake target already does this through `BUILD_DEB_PACKAGE=ON`.

---

## File Structure

- Modify `src/overlay/shapesmodel.h`: add `BindingsRole` and `AttachedTextRole`.
- Modify `src/overlay/shapesmodel.cpp`: expose `bindings` and `attachedText` roles; notify those roles when bindings change.
- Modify `src/overlay/overlaycontroller.h`: add QML invokables and private helpers for attached-text bindings and clipboard conversion.
- Modify `src/overlay/overlaycontroller.cpp`: implement attached-text binding helpers, update selection/default font behavior, update copy/paste conversion.
- Modify `src/overlay/qml/main.qml`: generalize the text editor for standalone text and attached text; add editor placement helpers.
- Modify `src/overlay/qml/shapes/BaseShape.qml`: render attached text and route double-clicks to attached-text editing for rectangle/ellipse/line/arrow.
- Modify `tests/shapesmodeltest.cpp`: add focused model/controller/clipboard tests.

---

### Task 1: Model Roles and Controller Attached-Text API

**Files:**
- Modify: `tests/shapesmodeltest.cpp`
- Modify: `src/overlay/shapesmodel.h`
- Modify: `src/overlay/shapesmodel.cpp`
- Modify: `src/overlay/overlaycontroller.h`
- Modify: `src/overlay/overlaycontroller.cpp`

**Interfaces:**
- Consumes: existing `ShapesModel::updateShape`, `OverlayController::addShape`, `OverlayController::getShape`.
- Produces:
  - `ShapesModel::BindingsRole`
  - `ShapesModel::AttachedTextRole`
  - QML/C++ invokable `QVariantMap OverlayController::attachedTextForShape(int index) const`
  - QML/C++ invokable `QVariantMap OverlayController::ensureAttachedTextForShape(int index)`
  - QML/C++ invokable `void OverlayController::setAttachedText(int index, const QString &text)`
  - QML/C++ invokable `void OverlayController::removeAttachedText(int index)`

- [ ] **Step 1: Add failing test declarations**

In `tests/shapesmodeltest.cpp`, add these slots after `testExcalidrawBindingRoundtrip();`:

```cpp
    void testAttachedTextDataModel();
    void testAttachedTextUndoRedo();
```

- [ ] **Step 2: Add failing attached-text model test**

Append this test near the existing binding tests in `tests/shapesmodeltest.cpp`:

```cpp
void ShapesModelTest::testAttachedTextDataModel()
{
    OverlayController controller;

    QVariantMap rect;
    rect[QStringLiteral("type")] = QStringLiteral("rectangle");
    rect[QStringLiteral("id")] = QStringLiteral("rect-attached");
    rect[QStringLiteral("x")] = 10.0;
    rect[QStringLiteral("y")] = 20.0;
    rect[QStringLiteral("width")] = 100.0;
    rect[QStringLiteral("height")] = 60.0;
    rect[QStringLiteral("color")] = QStringLiteral("#123456");
    rect[QStringLiteral("opacity")] = 0.75;
    rect[QStringLiteral("fontFamily")] = QStringLiteral("monospace");
    rect[QStringLiteral("fontSize")] = 24;
    controller.addShape(rect);

    QVariantMap created = controller.ensureAttachedTextForShape(0);
    QCOMPARE(created.value(QStringLiteral("type")).toString(), QStringLiteral("attachedText"));
    QVERIFY(!created.value(QStringLiteral("id")).toString().isEmpty());
    QCOMPARE(created.value(QStringLiteral("text")).toString(), QString());
    QCOMPARE(created.value(QStringLiteral("fontFamily")).toString(), QStringLiteral("monospace"));
    QCOMPARE(created.value(QStringLiteral("fontSize")).toInt(), 24);
    QCOMPARE(created.value(QStringLiteral("textAlign")).toString(), QStringLiteral("center"));
    QCOMPARE(created.value(QStringLiteral("verticalAlign")).toString(), QStringLiteral("middle"));

    controller.setAttachedText(0, QStringLiteral("Hello container"));
    QVariantMap attached = controller.attachedTextForShape(0);
    QCOMPARE(attached.value(QStringLiteral("text")).toString(), QStringLiteral("Hello container"));

    QModelIndex idx = controller.shapesModel()->index(0);
    QVariantMap roleAttached = controller.shapesModel()->data(idx, ShapesModel::AttachedTextRole).toMap();
    QCOMPARE(roleAttached.value(QStringLiteral("text")).toString(), QStringLiteral("Hello container"));
    QVariantList bindings = controller.shapesModel()->data(idx, ShapesModel::BindingsRole).toList();
    QCOMPARE(bindings.size(), 1);

    QCOMPARE(controller.shapesModel()->rowCount(), 1);

    controller.removeAttachedText(0);
    QVERIFY(controller.attachedTextForShape(0).isEmpty());
    QCOMPARE(controller.shapesModel()->data(idx, ShapesModel::BindingsRole).toList().size(), 0);
}
```

- [ ] **Step 3: Add failing undo/redo test**

Append this test near `testAttachedTextDataModel()`:

```cpp
void ShapesModelTest::testAttachedTextUndoRedo()
{
    OverlayController controller;

    QVariantMap arrow;
    arrow[QStringLiteral("type")] = QStringLiteral("arrow");
    arrow[QStringLiteral("id")] = QStringLiteral("arrow-attached");
    arrow[QStringLiteral("fromX")] = 10.0;
    arrow[QStringLiteral("fromY")] = 20.0;
    arrow[QStringLiteral("toX")] = 130.0;
    arrow[QStringLiteral("toY")] = 80.0;
    arrow[QStringLiteral("color")] = QStringLiteral("#abcdef");
    arrow[QStringLiteral("fontFamily")] = QStringLiteral("sans-serif");
    arrow[QStringLiteral("fontSize")] = 18;
    controller.addShape(arrow);

    controller.setAttachedText(0, QStringLiteral("First"));
    QCOMPARE(controller.attachedTextForShape(0).value(QStringLiteral("text")).toString(), QStringLiteral("First"));

    controller.undo();
    QVERIFY(controller.attachedTextForShape(0).isEmpty());

    controller.redo();
    QCOMPARE(controller.attachedTextForShape(0).value(QStringLiteral("text")).toString(), QStringLiteral("First"));

    controller.setAttachedText(0, QStringLiteral("Second"));
    QCOMPARE(controller.attachedTextForShape(0).value(QStringLiteral("text")).toString(), QStringLiteral("Second"));

    controller.undo();
    QCOMPARE(controller.attachedTextForShape(0).value(QStringLiteral("text")).toString(), QStringLiteral("First"));
}
```

- [ ] **Step 4: Run the focused failing tests**

Run:

```bash
cmake --build build-deb --target shapesmodeltest --parallel
QT_QPA_PLATFORM=offscreen ./build-deb/tests/shapesmodeltest testAttachedTextDataModel testAttachedTextUndoRedo
```

Expected: build fails because `AttachedTextRole`, `BindingsRole`, and controller methods do not exist.

- [ ] **Step 5: Add model roles**

In `src/overlay/shapesmodel.h`, extend `ShapeRoles` after `BoundElementIdsRole`:

```cpp
        BoundElementIdsRole,
        BindingsRole,
        AttachedTextRole
```

In `src/overlay/shapesmodel.cpp`, add a local helper near `normalizePoints`:

```cpp
static QVariantMap firstAttachedTextBinding(const QVariantMap &shape)
{
    const QVariantList bindings = shape.value(QStringLiteral("bindings")).toList();
    for (const QVariant &bindingValue : bindings) {
        const QVariantMap binding = bindingValue.toMap();
        if (binding.value(QStringLiteral("type")).toString() == QStringLiteral("attachedText")) {
            return binding;
        }
    }
    return QVariantMap();
}
```

Add cases in `ShapesModel::data()`:

```cpp
        case BindingsRole: return shape.value(QStringLiteral("bindings"));
        case AttachedTextRole: return firstAttachedTextBinding(shape);
```

Add names in `ShapesModel::roleNames()`:

```cpp
    roles[BindingsRole] = "bindings";
    roles[AttachedTextRole] = "attachedText";
```

Update `ShapesModel::updateShape()` role mapping:

```cpp
                else if (it.key() == QStringLiteral("bindings")) changedRoles << BindingsRole << AttachedTextRole;
```

- [ ] **Step 6: Add controller declarations**

In `src/overlay/overlaycontroller.h`, add public invokables after `getShape(int index) const`:

```cpp
    Q_INVOKABLE QVariantMap attachedTextForShape(int index) const;
    Q_INVOKABLE QVariantMap ensureAttachedTextForShape(int index);
    Q_INVOKABLE void setAttachedText(int index, const QString &text);
    Q_INVOKABLE void removeAttachedText(int index);
```

Add private helpers after `convertFromExcalidraw`:

```cpp
    static bool supportsAttachedText(const QString &type);
    static QVariantMap firstAttachedTextBinding(const QVariantMap &shape);
    static QVariantList withoutAttachedTextBinding(const QVariantMap &shape);
    QVariantMap makeAttachedTextBinding(const QVariantMap &shape) const;
```

- [ ] **Step 7: Implement controller helpers**

In `src/overlay/overlaycontroller.cpp`, add these helpers near `getShape()`:

```cpp
bool OverlayController::supportsAttachedText(const QString &type)
{
    return type == QStringLiteral("rectangle")
        || type == QStringLiteral("ellipse")
        || type == QStringLiteral("line")
        || type == QStringLiteral("arrow");
}

QVariantMap OverlayController::firstAttachedTextBinding(const QVariantMap &shape)
{
    const QVariantList bindings = shape.value(QStringLiteral("bindings")).toList();
    for (const QVariant &bindingValue : bindings) {
        const QVariantMap binding = bindingValue.toMap();
        if (binding.value(QStringLiteral("type")).toString() == QStringLiteral("attachedText")) {
            return binding;
        }
    }
    return QVariantMap();
}

QVariantList OverlayController::withoutAttachedTextBinding(const QVariantMap &shape)
{
    QVariantList filtered;
    const QVariantList bindings = shape.value(QStringLiteral("bindings")).toList();
    for (const QVariant &bindingValue : bindings) {
        const QVariantMap binding = bindingValue.toMap();
        if (binding.value(QStringLiteral("type")).toString() != QStringLiteral("attachedText")) {
            filtered.append(binding);
        }
    }
    return filtered;
}

QVariantMap OverlayController::makeAttachedTextBinding(const QVariantMap &shape) const
{
    QVariantMap binding;
    binding.insert(QStringLiteral("type"), QStringLiteral("attachedText"));
    binding.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    binding.insert(QStringLiteral("text"), QString());
    binding.insert(QStringLiteral("fontFamily"), shape.value(QStringLiteral("fontFamily"), m_defaultFontFamily).toString());
    binding.insert(QStringLiteral("fontSize"), shape.value(QStringLiteral("fontSize"), m_defaultFontSize).toInt());
    binding.insert(QStringLiteral("textAlign"), QStringLiteral("center"));
    binding.insert(QStringLiteral("verticalAlign"), QStringLiteral("middle"));
    return binding;
}

QVariantMap OverlayController::attachedTextForShape(int index) const
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        return QVariantMap();
    }
    return firstAttachedTextBinding(m_shapesModel.shapes().at(index));
}

QVariantMap OverlayController::ensureAttachedTextForShape(int index)
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        return QVariantMap();
    }

    QVariantMap shape = m_shapesModel.shapes().at(index);
    const QString type = shape.value(QStringLiteral("type")).toString();
    if (!supportsAttachedText(type)) {
        return QVariantMap();
    }

    QVariantMap existing = firstAttachedTextBinding(shape);
    if (!existing.isEmpty()) {
        return existing;
    }

    QVariantMap binding = makeAttachedTextBinding(shape);
    QVariantList bindings = shape.value(QStringLiteral("bindings")).toList();
    bindings.append(binding);
    m_shapesModel.updateShape(index, {{QStringLiteral("bindings"), bindings}});
    notifyShapesChanged();
    if (index == m_selectedIndex) {
        notifySelectionChanged();
    }
    return binding;
}

void OverlayController::setAttachedText(int index, const QString &text)
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        return;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        removeAttachedText(index);
        return;
    }

    QVariantMap shape = m_shapesModel.shapes().at(index);
    if (!supportsAttachedText(shape.value(QStringLiteral("type")).toString())) {
        return;
    }

    QVariantList bindings = withoutAttachedTextBinding(shape);
    QVariantMap binding = firstAttachedTextBinding(shape);
    if (binding.isEmpty()) {
        binding = makeAttachedTextBinding(shape);
    }
    binding.insert(QStringLiteral("text"), trimmed);
    binding.insert(QStringLiteral("fontFamily"), shape.value(QStringLiteral("fontFamily"), m_defaultFontFamily).toString());
    binding.insert(QStringLiteral("fontSize"), shape.value(QStringLiteral("fontSize"), m_defaultFontSize).toInt());
    bindings.append(binding);

    m_shapesModel.updateShape(index, {{QStringLiteral("bindings"), bindings}});
    notifyShapesChanged();
    if (index == m_selectedIndex) {
        notifySelectionChanged();
    }
}

void OverlayController::removeAttachedText(int index)
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        return;
    }

    const QVariantMap shape = m_shapesModel.shapes().at(index);
    QVariantList bindings = withoutAttachedTextBinding(shape);
    if (bindings == shape.value(QStringLiteral("bindings")).toList()) {
        return;
    }

    m_shapesModel.updateShape(index, {{QStringLiteral("bindings"), bindings}});
    notifyShapesChanged();
    if (index == m_selectedIndex) {
        notifySelectionChanged();
    }
}
```

- [ ] **Step 8: Run the focused tests**

Run:

```bash
cmake --build build-deb --target shapesmodeltest --parallel
QT_QPA_PLATFORM=offscreen ./build-deb/tests/shapesmodeltest testAttachedTextDataModel testAttachedTextUndoRedo
```

Expected: both tests pass.

- [ ] **Step 9: Commit Task 1**

Run:

```bash
git add tests/shapesmodeltest.cpp src/overlay/shapesmodel.h src/overlay/shapesmodel.cpp src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp
git commit -m "feat: add attached text model bindings"
```

---

### Task 2: Excalidraw Clipboard Attached-Text Round-Trip

**Files:**
- Modify: `tests/shapesmodeltest.cpp`
- Modify: `src/overlay/overlaycontroller.h`
- Modify: `src/overlay/overlaycontroller.cpp`

**Interfaces:**
- Consumes: Task 1 attached-text controller API.
- Produces: clipboard conversion for attached text on rectangle, ellipse, arrow, and line.

- [ ] **Step 1: Add failing test declarations**

In `tests/shapesmodeltest.cpp`, add slots after `testAttachedTextUndoRedo();`:

```cpp
    void testAttachedTextExcalidrawExport();
    void testAttachedTextExcalidrawImport();
    void testLineAttachedTextClipboardFallback();
```

- [ ] **Step 2: Add failing export test**

Append:

```cpp
void ShapesModelTest::testAttachedTextExcalidrawExport()
{
    OverlayController controller;

    QVariantMap rect;
    rect[QStringLiteral("type")] = QStringLiteral("rectangle");
    rect[QStringLiteral("id")] = QStringLiteral("rect-export");
    rect[QStringLiteral("x")] = 10.0;
    rect[QStringLiteral("y")] = 20.0;
    rect[QStringLiteral("width")] = 120.0;
    rect[QStringLiteral("height")] = 80.0;
    rect[QStringLiteral("color")] = QStringLiteral("#111111");
    rect[QStringLiteral("opacity")] = 0.8;
    rect[QStringLiteral("fontFamily")] = QStringLiteral("monospace");
    rect[QStringLiteral("fontSize")] = 22;
    controller.addShape(rect);
    controller.setAttachedText(0, QStringLiteral("Bound label"));

    controller.copySelected();
    const QJsonObject root = QJsonDocument::fromJson(QGuiApplication::clipboard()->text().toUtf8()).object();
    const QJsonArray elements = root.value(QStringLiteral("elements")).toArray();
    QCOMPARE(elements.size(), 2);

    const QJsonObject exportedRect = elements.at(0).toObject();
    const QJsonObject exportedText = elements.at(1).toObject();
    QCOMPARE(exportedRect.value(QStringLiteral("type")).toString(), QStringLiteral("rectangle"));
    QCOMPARE(exportedText.value(QStringLiteral("type")).toString(), QStringLiteral("text"));
    QCOMPARE(exportedText.value(QStringLiteral("containerId")).toString(), QStringLiteral("rect-export"));
    QCOMPARE(exportedText.value(QStringLiteral("text")).toString(), QStringLiteral("Bound label"));
    QCOMPARE(exportedText.value(QStringLiteral("originalText")).toString(), QStringLiteral("Bound label"));
    QCOMPARE(exportedText.value(QStringLiteral("textAlign")).toString(), QStringLiteral("center"));
    QCOMPARE(exportedText.value(QStringLiteral("verticalAlign")).toString(), QStringLiteral("middle"));
    QVERIFY(exportedText.value(QStringLiteral("autoResize")).toBool());

    const QJsonArray boundElements = exportedRect.value(QStringLiteral("boundElements")).toArray();
    QCOMPARE(boundElements.size(), 1);
    QCOMPARE(boundElements.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("text"));
    QCOMPARE(boundElements.at(0).toObject().value(QStringLiteral("id")).toString(), exportedText.value(QStringLiteral("id")).toString());
}
```

- [ ] **Step 3: Add failing import test**

Append:

```cpp
void ShapesModelTest::testAttachedTextExcalidrawImport()
{
    OverlayController controller;

    QJsonObject root;
    root.insert(QStringLiteral("type"), QStringLiteral("excalidraw/clipboard"));
    QJsonArray elements;

    QJsonObject rect;
    rect.insert(QStringLiteral("id"), QStringLiteral("rect-import"));
    rect.insert(QStringLiteral("type"), QStringLiteral("rectangle"));
    rect.insert(QStringLiteral("x"), 10.0);
    rect.insert(QStringLiteral("y"), 20.0);
    rect.insert(QStringLiteral("width"), 120.0);
    rect.insert(QStringLiteral("height"), 80.0);
    rect.insert(QStringLiteral("strokeColor"), QStringLiteral("#222222"));
    rect.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
    rect.insert(QStringLiteral("strokeWidth"), 2.0);
    rect.insert(QStringLiteral("opacity"), 100.0);
    rect.insert(QStringLiteral("roughness"), 1);
    rect.insert(QStringLiteral("seed"), 12345);
    rect.insert(QStringLiteral("locked"), false);
    QJsonArray bound;
    bound.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("text-import")}, {QStringLiteral("type"), QStringLiteral("text")}});
    rect.insert(QStringLiteral("boundElements"), bound);
    elements.append(rect);

    QJsonObject text;
    text.insert(QStringLiteral("id"), QStringLiteral("text-import"));
    text.insert(QStringLiteral("type"), QStringLiteral("text"));
    text.insert(QStringLiteral("x"), 25.0);
    text.insert(QStringLiteral("y"), 35.0);
    text.insert(QStringLiteral("width"), 80.0);
    text.insert(QStringLiteral("height"), 30.0);
    text.insert(QStringLiteral("strokeColor"), QStringLiteral("#222222"));
    text.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
    text.insert(QStringLiteral("strokeWidth"), 1.0);
    text.insert(QStringLiteral("opacity"), 100.0);
    text.insert(QStringLiteral("roughness"), 1);
    text.insert(QStringLiteral("seed"), 67890);
    text.insert(QStringLiteral("locked"), false);
    text.insert(QStringLiteral("containerId"), QStringLiteral("rect-import"));
    text.insert(QStringLiteral("text"), QStringLiteral("Imported label"));
    text.insert(QStringLiteral("originalText"), QStringLiteral("Imported label"));
    text.insert(QStringLiteral("fontSize"), 20.0);
    text.insert(QStringLiteral("fontFamily"), 3.0);
    text.insert(QStringLiteral("textAlign"), QStringLiteral("center"));
    text.insert(QStringLiteral("verticalAlign"), QStringLiteral("middle"));
    text.insert(QStringLiteral("autoResize"), true);
    elements.append(text);

    root.insert(QStringLiteral("elements"), elements);
    QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));

    controller.pasteFromClipboard(100.0, 100.0);
    QCOMPARE(controller.shapesModel()->rowCount(), 1);
    QVariantMap pasted = controller.getShape(0);
    QCOMPARE(pasted.value(QStringLiteral("type")).toString(), QStringLiteral("rectangle"));
    QVariantMap attached = controller.attachedTextForShape(0);
    QCOMPARE(attached.value(QStringLiteral("id")).toString(), QStringLiteral("text-import"));
    QCOMPARE(attached.value(QStringLiteral("text")).toString(), QStringLiteral("Imported label"));
    QCOMPARE(attached.value(QStringLiteral("fontFamily")).toString(), QStringLiteral("Cascadia Code"));
    QCOMPARE(attached.value(QStringLiteral("fontSize")).toInt(), 20);
}
```

- [ ] **Step 4: Add failing line fallback test**

Append:

```cpp
void ShapesModelTest::testLineAttachedTextClipboardFallback()
{
    OverlayController controller;

    QVariantMap line;
    line[QStringLiteral("type")] = QStringLiteral("line");
    line[QStringLiteral("id")] = QStringLiteral("line-export");
    line[QStringLiteral("fromX")] = 10.0;
    line[QStringLiteral("fromY")] = 20.0;
    line[QStringLiteral("toX")] = 110.0;
    line[QStringLiteral("toY")] = 20.0;
    line[QStringLiteral("color")] = QStringLiteral("#333333");
    line[QStringLiteral("opacity")] = 1.0;
    line[QStringLiteral("fontFamily")] = QStringLiteral("sans-serif");
    line[QStringLiteral("fontSize")] = 18;
    controller.addShape(line);
    controller.setAttachedText(0, QStringLiteral("Line label"));

    controller.copySelected();
    const QJsonObject root = QJsonDocument::fromJson(QGuiApplication::clipboard()->text().toUtf8()).object();
    const QJsonArray elements = root.value(QStringLiteral("elements")).toArray();
    QCOMPARE(elements.size(), 2);
    QCOMPARE(elements.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("line"));

    const QJsonObject text = elements.at(1).toObject();
    QCOMPARE(text.value(QStringLiteral("type")).toString(), QStringLiteral("text"));
    QCOMPARE(text.value(QStringLiteral("containerId")).toString(), QString());
    QCOMPARE(text.value(QStringLiteral("text")).toString(), QStringLiteral("Line label"));
    QCOMPARE(text.value(QStringLiteral("customData")).toObject()
                 .value(QStringLiteral("scribbleway")).toObject()
                 .value(QStringLiteral("attachedTextFor")).toString(), QStringLiteral("line-export"));

    OverlayController pasted;
    QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    pasted.pasteFromClipboard(200.0, 200.0);
    QCOMPARE(pasted.shapesModel()->rowCount(), 1);
    QCOMPARE(pasted.attachedTextForShape(0).value(QStringLiteral("text")).toString(), QStringLiteral("Line label"));
}
```

- [ ] **Step 5: Run failing clipboard tests**

Run:

```bash
cmake --build build-deb --target shapesmodeltest --parallel
QT_QPA_PLATFORM=offscreen ./build-deb/tests/shapesmodeltest testAttachedTextExcalidrawExport testAttachedTextExcalidrawImport testLineAttachedTextClipboardFallback
```

Expected: tests fail because copy/paste ignores attached-text bindings.

- [ ] **Step 6: Add conversion helper declarations**

In `src/overlay/overlaycontroller.h`, replace the private conversion declarations with:

```cpp
    QJsonObject convertToExcalidraw(const QVariantMap &shape);
    QJsonObject convertAttachedTextToExcalidraw(const QVariantMap &shape, const QVariantMap &attachedText) const;
    QVariantMap convertFromExcalidraw(const QJsonObject &elem);
    void attachImportedBoundText(QList<QVariantMap> &shapes) const;
```

- [ ] **Step 7: Export attached text from selected containers**

In `OverlayController::copySelected()`, after appending a selected shape's container object, append attached text:

```cpp
            QJsonObject elemObj = convertToExcalidraw(shape);
            if (!elemObj.isEmpty()) {
                elements.append(elemObj);
                const QVariantMap attached = firstAttachedTextBinding(shape);
                if (!attached.isEmpty()) {
                    const QJsonObject textObj = convertAttachedTextToExcalidraw(shape, attached);
                    if (!textObj.isEmpty()) {
                        elements.append(textObj);
                    }
                }
            }
```

In `convertToExcalidraw()`, when `type` is `rectangle`, `ellipse`, or `arrow`, merge the attached text id into `boundElements` with type `text`. Preserve existing arrow ids already stored in `boundElementIds` for rectangle/ellipse.

- [ ] **Step 8: Implement attached-text JSON export**

Add this function in `src/overlay/overlaycontroller.cpp` after `convertToExcalidraw()`:

```cpp
QJsonObject OverlayController::convertAttachedTextToExcalidraw(const QVariantMap &shape, const QVariantMap &attachedText) const
{
    QJsonObject elem;
    const QString containerType = shape.value(QStringLiteral("type")).toString();
    const QString text = attachedText.value(QStringLiteral("text")).toString();
    if (text.trimmed().isEmpty()) {
        return elem;
    }

    QString id = attachedText.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    elem.insert(QStringLiteral("id"), id);
    elem.insert(QStringLiteral("type"), QStringLiteral("text"));
    elem.insert(QStringLiteral("strokeColor"), shape.value(QStringLiteral("color"), QStringLiteral("#000000")).toString());
    elem.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
    elem.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
    elem.insert(QStringLiteral("strokeWidth"), 1.0);
    elem.insert(QStringLiteral("strokeStyle"), QStringLiteral("solid"));
    elem.insert(QStringLiteral("roughness"), shape.value(QStringLiteral("roughness"), 1).toInt());
    elem.insert(QStringLiteral("opacity"), qRound(shape.value(QStringLiteral("opacity"), 1.0).toDouble() * 100.0));
    elem.insert(QStringLiteral("angle"), 0.0);
    elem.insert(QStringLiteral("isDeleted"), false);
    elem.insert(QStringLiteral("seed"), attachedText.value(QStringLiteral("seed"), 123456).toInt());
    elem.insert(QStringLiteral("version"), 1);
    elem.insert(QStringLiteral("versionNonce"), 123456789);
    elem.insert(QStringLiteral("updated"), 0);
    elem.insert(QStringLiteral("locked"), shape.value(QStringLiteral("locked"), false).toBool());
    elem.insert(QStringLiteral("text"), text);
    elem.insert(QStringLiteral("originalText"), text);
    elem.insert(QStringLiteral("fontSize"), shape.value(QStringLiteral("fontSize"), attachedText.value(QStringLiteral("fontSize"), m_defaultFontSize)).toInt());
    elem.insert(QStringLiteral("fontFamily"), 2);
    elem.insert(QStringLiteral("textAlign"), QStringLiteral("center"));
    elem.insert(QStringLiteral("verticalAlign"), QStringLiteral("middle"));
    elem.insert(QStringLiteral("autoResize"), true);
    elem.insert(QStringLiteral("lineHeight"), 1.25);

    double x = 0.0;
    double y = 0.0;
    double width = 100.0;
    double height = qMax(24, elem.value(QStringLiteral("fontSize")).toInt());
    if (containerType == QStringLiteral("rectangle") || containerType == QStringLiteral("ellipse")) {
        x = shape.value(QStringLiteral("x")).toDouble();
        y = shape.value(QStringLiteral("y")).toDouble();
        width = shape.value(QStringLiteral("width")).toDouble();
        height = shape.value(QStringLiteral("height")).toDouble();
        elem.insert(QStringLiteral("containerId"), shape.value(QStringLiteral("id")).toString());
    } else if (containerType == QStringLiteral("arrow")) {
        const double fromX = shape.value(QStringLiteral("fromX")).toDouble();
        const double fromY = shape.value(QStringLiteral("fromY")).toDouble();
        const double toX = shape.value(QStringLiteral("toX")).toDouble();
        const double toY = shape.value(QStringLiteral("toY")).toDouble();
        width = qMax(80.0, qAbs(toX - fromX) * 0.7);
        x = (fromX + toX) / 2.0 - width / 2.0;
        y = (fromY + toY) / 2.0 - height / 2.0;
        elem.insert(QStringLiteral("containerId"), shape.value(QStringLiteral("id")).toString());
    } else if (containerType == QStringLiteral("line")) {
        const double fromX = shape.value(QStringLiteral("fromX")).toDouble();
        const double fromY = shape.value(QStringLiteral("fromY")).toDouble();
        const double toX = shape.value(QStringLiteral("toX")).toDouble();
        const double toY = shape.value(QStringLiteral("toY")).toDouble();
        width = qMax(80.0, qAbs(toX - fromX) * 0.7);
        x = (fromX + toX) / 2.0 - width / 2.0;
        y = (fromY + toY) / 2.0 - height / 2.0;
        elem.insert(QStringLiteral("containerId"), QString());
        elem.insert(QStringLiteral("customData"), QJsonObject{
            {QStringLiteral("scribbleway"), QJsonObject{{QStringLiteral("attachedTextFor"), shape.value(QStringLiteral("id")).toString()}}}
        });
    }

    elem.insert(QStringLiteral("x"), x);
    elem.insert(QStringLiteral("y"), y);
    elem.insert(QStringLiteral("width"), width);
    elem.insert(QStringLiteral("height"), height);
    return elem;
}
```

- [ ] **Step 9: Preserve text binding metadata during import**

In `convertFromExcalidraw()`, preserve `containerId` and Scribbleway `customData` on imported text shapes so the paste phase can attach them to their containers:

```cpp
        } else if (type == QStringLiteral("text")) {
            shape.insert(QStringLiteral("text"), elem.value(QStringLiteral("text")).toString());
            shape.insert(QStringLiteral("fontSize"), static_cast<int>(elem.value(QStringLiteral("fontSize")).toDouble(20.0)));

            int excalFont = static_cast<int>(elem.value(QStringLiteral("fontFamily")).toDouble(2.0));
            QString family;
            if (excalFont == 3) {
                family = QStringLiteral("Cascadia Code");
            } else if (excalFont == 1) {
                family = QStringLiteral("Virgil");
            } else {
                family = QStringLiteral("sans-serif");
            }
            shape.insert(QStringLiteral("fontFamily"), family);

            const QString containerId = elem.value(QStringLiteral("containerId")).toString();
            if (!containerId.isEmpty()) {
                shape.insert(QStringLiteral("containerId"), containerId);
            }
            const QJsonValue customData = elem.value(QStringLiteral("customData"));
            if (customData.isObject()) {
                shape.insert(QStringLiteral("customData"), customData.toObject().toVariantMap());
            }
        }
```

- [ ] **Step 10: Convert imported bound text into container bindings**

In `pasteFromClipboard()`, keep converted text elements in the temporary list, then call this before ID remapping:

```cpp
    QList<QVariantMap> imported;
    for (const QVariant &item : list) {
        imported.append(item.toMap());
    }
    attachImportedBoundText(imported);
    list.clear();
    for (const QVariantMap &shape : imported) {
        list.append(shape);
    }
```

Implement `attachImportedBoundText(QList<QVariantMap> &shapes) const`:

```cpp
void OverlayController::attachImportedBoundText(QList<QVariantMap> &shapes) const
{
    QHash<QString, int> containerIndexById;
    for (int i = 0; i < shapes.size(); ++i) {
        const QString type = shapes[i].value(QStringLiteral("type")).toString();
        const QString id = shapes[i].value(QStringLiteral("id")).toString();
        if (supportsAttachedText(type) && !id.isEmpty()) {
            containerIndexById.insert(id, i);
        }
    }

    QList<int> textIndicesToRemove;
    for (int i = 0; i < shapes.size(); ++i) {
        QVariantMap textShape = shapes[i];
        if (textShape.value(QStringLiteral("type")).toString() != QStringLiteral("text")) {
            continue;
        }

        QString containerId = textShape.value(QStringLiteral("containerId")).toString();
        if (containerId.isEmpty()) {
            containerId = textShape.value(QStringLiteral("customData")).toMap()
                .value(QStringLiteral("scribbleway")).toMap()
                .value(QStringLiteral("attachedTextFor")).toString();
        }
        if (!containerIndexById.contains(containerId)) {
            continue;
        }

        const int containerIndex = containerIndexById.value(containerId);
        QVariantMap container = shapes[containerIndex];
        QVariantMap binding;
        binding.insert(QStringLiteral("type"), QStringLiteral("attachedText"));
        binding.insert(QStringLiteral("id"), textShape.value(QStringLiteral("id")).toString());
        binding.insert(QStringLiteral("text"), textShape.value(QStringLiteral("text")).toString());
        binding.insert(QStringLiteral("fontFamily"), textShape.value(QStringLiteral("fontFamily"), container.value(QStringLiteral("fontFamily"), m_defaultFontFamily)).toString());
        binding.insert(QStringLiteral("fontSize"), textShape.value(QStringLiteral("fontSize"), container.value(QStringLiteral("fontSize"), m_defaultFontSize)).toInt());
        binding.insert(QStringLiteral("textAlign"), QStringLiteral("center"));
        binding.insert(QStringLiteral("verticalAlign"), QStringLiteral("middle"));

        QVariantList bindings = withoutAttachedTextBinding(container);
        bindings.append(binding);
        container.insert(QStringLiteral("bindings"), bindings);
        container.insert(QStringLiteral("fontFamily"), binding.value(QStringLiteral("fontFamily")));
        container.insert(QStringLiteral("fontSize"), binding.value(QStringLiteral("fontSize")));
        shapes[containerIndex] = container;
        textIndicesToRemove.prepend(i);
    }

    for (int index : textIndicesToRemove) {
        shapes.removeAt(index);
    }
}
```

- [ ] **Step 11: Run clipboard tests**

Run:

```bash
cmake --build build-deb --target shapesmodeltest --parallel
QT_QPA_PLATFORM=offscreen ./build-deb/tests/shapesmodeltest testAttachedTextExcalidrawExport testAttachedTextExcalidrawImport testLineAttachedTextClipboardFallback
```

Expected: all three tests pass.

- [ ] **Step 12: Commit Task 2**

Run:

```bash
git add tests/shapesmodeltest.cpp src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp
git commit -m "feat: round-trip attached text clipboard data"
```

---

### Task 3: QML Rendering and Editing

**Files:**
- Modify: `src/overlay/qml/main.qml`
- Modify: `src/overlay/qml/shapes/BaseShape.qml`

**Interfaces:**
- Consumes: Task 1 controller API and `model.attachedText` role.
- Produces:
  - `canvasWindow.startAttachedTextEditing(shapeIndex)`
  - attached text rendered by `BaseShape.qml`

- [ ] **Step 1: Build before QML edits**

Run:

```bash
cmake --build build-deb --target scribbleway-overlay --parallel
```

Expected: build succeeds before QML changes.

- [ ] **Step 2: Add editor target state in `main.qml`**

Replace the current text editing state near the top of `src/overlay/qml/main.qml`:

```qml
    // Text editing index
    property int editingShapeIndex: -1
```

with:

```qml
    // Text editing target: "standalone" edits a text shape, "attached" edits a container binding.
    property int editingShapeIndex: -1
    property string editingTextKind: ""
```

- [ ] **Step 3: Add attached text geometry helpers in `main.qml`**

Add these functions before `startTextEditing(shapeIndex)`:

```qml
    function attachedTextRect(shape) {
        const padding = 12;
        const type = (shape.type || "").toLowerCase();
        if (type === "rectangle") {
            return Qt.rect(shape.x + padding, shape.y + padding,
                           Math.max(40, shape.width - padding * 2),
                           Math.max(24, shape.height - padding * 2));
        }
        if (type === "ellipse") {
            const insetX = shape.width * (1 - Math.SQRT1_2) / 2 + padding;
            const insetY = shape.height * (1 - Math.SQRT1_2) / 2 + padding;
            return Qt.rect(shape.x + insetX, shape.y + insetY,
                           Math.max(40, shape.width - insetX * 2),
                           Math.max(24, shape.height - insetY * 2));
        }
        if (type === "line" || type === "arrow") {
            const midX = (shape.fromX + shape.toX) / 2;
            const midY = (shape.fromY + shape.toY) / 2;
            const len = Math.sqrt(Math.pow(shape.toX - shape.fromX, 2) + Math.pow(shape.toY - shape.fromY, 2));
            const labelWidth = Math.max(80, len * 0.7);
            const labelHeight = Math.max(32, (shape.fontSize || controller.defaultFontSize) * 1.6);
            return Qt.rect(midX - labelWidth / 2, midY - labelHeight / 2, labelWidth, labelHeight);
        }
        return Qt.rect(0, 0, 120, 40);
    }
```

- [ ] **Step 4: Generalize commit logic in `textEditor`**

Replace `textEditor.commitText()` with:

```qml
        function commitText() {
            if (editingShapeIndex !== -1) {
                let val = text.trim();
                if (editingTextKind === "attached") {
                    if (val === "") {
                        controller.removeAttachedText(editingShapeIndex);
                    } else {
                        controller.setAttachedText(editingShapeIndex, val);
                    }
                } else {
                    if (val === "") {
                        controller.deleteShape(editingShapeIndex);
                    } else {
                        controller.updateShape(editingShapeIndex, { "text": val });
                    }
                }
            }
            visible = false;
            editingShapeIndex = -1;
            editingTextKind = "";
            canvasWindow.requestInputRegionUpdate();
        }
```

- [ ] **Step 5: Keep standalone text editing unchanged**

Update `startTextEditing(shapeIndex)` to set `editingTextKind`:

```qml
    function startTextEditing(shapeIndex) {
        let shape = controller.getShape(shapeIndex);
        if (!shape) return;

        editingShapeIndex = shapeIndex;
        editingTextKind = "standalone";
        controller.setSelectedIndex(shapeIndex);

        textEditor.x = shape.x;
        textEditor.y = shape.y;
        textEditor.text = shape.text || "";
        textEditor.visible = true;

        canvasWindow.requestInputRegionUpdate();
    }
```

- [ ] **Step 6: Add attached text editing entry point**

Add after `startTextEditing(shapeIndex)`:

```qml
    function startAttachedTextEditing(shapeIndex) {
        let shape = controller.getShape(shapeIndex);
        if (!shape) return;

        let attached = controller.ensureAttachedTextForShape(shapeIndex);
        if (!attached) return;

        editingShapeIndex = shapeIndex;
        editingTextKind = "attached";
        controller.setSelectedIndex(shapeIndex);

        const r = attachedTextRect(shape);
        textEditor.x = r.x;
        textEditor.y = r.y;
        textEditor.width = r.width;
        textEditor.text = attached.text || "";
        textEditor.visible = true;

        canvasWindow.requestInputRegionUpdate();
    }
```

- [ ] **Step 7: Update input mask for attached editor**

Keep `requestInputRegionUpdate()` unchanged except that editor geometry now comes from `textEditor`. Verify this branch remains present:

```qml
        } else if (isPassthrough && textEditor.visible) {
            controller.updateInputMask([Qt.rect(textEditor.x - 5, textEditor.y - 5, textEditor.width + 10, textEditor.height + 10)]);
```

- [ ] **Step 8: Add attached text rendering properties in `BaseShape.qml`**

In `BaseShape.qml`, after model aliases, add:

```qml
    property var modelAttachedText: model.attachedText !== undefined ? model.attachedText : ({})
    readonly property bool hasAttachedText: modelAttachedText && modelAttachedText.text !== undefined && modelAttachedText.text !== ""
    readonly property bool supportsAttachedText: {
        const t = model && model.type ? model.type.toLowerCase() : "";
        return t === "rectangle" || t === "ellipse" || t === "line" || t === "arrow";
    }
    readonly property real attachedPadding: 12
    readonly property real attachedLineLength: Math.sqrt(Math.pow(shapeToX - shapeFromX, 2) + Math.pow(shapeToY - shapeFromY, 2))
```

- [ ] **Step 9: Render attached text in `BaseShape.qml`**

Insert this `Text` item after `shapeContent` and before the `MouseArea`:

```qml
    Text {
        id: attachedTextLabel
        visible: baseShapeRoot.hasAttachedText
        enabled: false
        z: 2
        text: baseShapeRoot.modelAttachedText.text || ""
        color: baseShapeRoot.modelColor
        opacity: baseShapeRoot.modelOpacity
        font.family: model.fontFamily !== undefined ? model.fontFamily : controller.defaultFontFamily
        font.pixelSize: model.fontSize !== undefined ? model.fontSize : controller.defaultFontSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        clip: false

        x: {
            if (baseShapeRoot.mode === "line") {
                return (baseShapeRoot.shapeFromX + baseShapeRoot.shapeToX) / 2 - width / 2;
            }
            if (model && model.type && model.type.toLowerCase() === "ellipse") {
                return baseShapeRoot.shapeX + baseShapeRoot.shapeWidth * (1 - Math.SQRT1_2) / 2 + baseShapeRoot.attachedPadding;
            }
            return baseShapeRoot.shapeX + baseShapeRoot.attachedPadding;
        }
        y: {
            if (baseShapeRoot.mode === "line") {
                return (baseShapeRoot.shapeFromY + baseShapeRoot.shapeToY) / 2 - height / 2;
            }
            if (model && model.type && model.type.toLowerCase() === "ellipse") {
                return baseShapeRoot.shapeY + baseShapeRoot.shapeHeight * (1 - Math.SQRT1_2) / 2 + baseShapeRoot.attachedPadding;
            }
            return baseShapeRoot.shapeY + baseShapeRoot.attachedPadding;
        }
        width: {
            if (baseShapeRoot.mode === "line") {
                return Math.max(80, baseShapeRoot.attachedLineLength * 0.7);
            }
            if (model && model.type && model.type.toLowerCase() === "ellipse") {
                const inset = baseShapeRoot.shapeWidth * (1 - Math.SQRT1_2) / 2 + baseShapeRoot.attachedPadding;
                return Math.max(40, baseShapeRoot.shapeWidth - inset * 2);
            }
            return Math.max(40, baseShapeRoot.shapeWidth - baseShapeRoot.attachedPadding * 2);
        }
        height: {
            if (baseShapeRoot.mode === "line") {
                return Math.max(32, font.pixelSize * 1.6);
            }
            if (model && model.type && model.type.toLowerCase() === "ellipse") {
                const inset = baseShapeRoot.shapeHeight * (1 - Math.SQRT1_2) / 2 + baseShapeRoot.attachedPadding;
                return Math.max(24, baseShapeRoot.shapeHeight - inset * 2);
            }
            return Math.max(24, baseShapeRoot.shapeHeight - baseShapeRoot.attachedPadding * 2);
        }
    }
```

- [ ] **Step 10: Route double-clicks to attached editor**

Replace `BaseShape.qml` `onDoubleClicked` handler:

```qml
        onDoubleClicked: (mouse) => {
            baseShapeRoot.doubleClicked(mouse);
        }
```

with:

```qml
        onDoubleClicked: (mouse) => {
            if (baseShapeRoot.supportsAttachedText && typeof canvasWindow !== "undefined") {
                canvasWindow.startAttachedTextEditing(baseShapeRoot.shapeIndex);
            } else {
                baseShapeRoot.doubleClicked(mouse);
            }
        }
```

- [ ] **Step 11: Build QML target**

Run:

```bash
cmake --build build-deb --target scribbleway-overlay --parallel
```

Expected: build succeeds; QML resources compile.

- [ ] **Step 12: Run focused controller tests after QML changes**

Run:

```bash
QT_QPA_PLATFORM=offscreen ./build-deb/tests/shapesmodeltest testAttachedTextDataModel testAttachedTextUndoRedo testAttachedTextExcalidrawExport testAttachedTextExcalidrawImport testLineAttachedTextClipboardFallback
```

Expected: all focused tests pass.

- [ ] **Step 13: Commit Task 3**

Run:

```bash
git add src/overlay/qml/main.qml src/overlay/qml/shapes/BaseShape.qml
git commit -m "feat: edit and render attached shape text"
```

---

### Task 4: Full Verification and Debian Package

**Files:**
- No source changes expected.

**Interfaces:**
- Consumes: Tasks 1-3.
- Produces: verified build, tests, and rebuilt `.deb` package.

- [ ] **Step 1: Run full test binary**

Run:

```bash
cmake --build build-deb --target shapesmodeltest --parallel
QT_QPA_PLATFORM=offscreen ./build-deb/tests/shapesmodeltest
```

Expected: all Qt tests pass.

- [ ] **Step 2: Run full build and rebuild `.deb`**

Run:

```bash
cmake --build build-deb --parallel
```

Expected: build succeeds and the existing `rebuild_debian_package` target refreshes `scribbleway_0.1-1_amd64.deb`.

- [ ] **Step 3: Smoke test overlay behavior**

Run the overlay from the build output:

```bash
./build-deb/bin/scribbleway-overlay
```

Exercise these paths:

1. Draw a rectangle, double-click it, type `Rect label`, press Enter.
2. Move the rectangle; label moves with it.
3. Change stroke color and font size; label changes with the container.
4. Double-click the rectangle again; existing text is editable.
5. Clear the text and press Enter; rectangle remains and label disappears.
6. Repeat create/edit/move/color checks for ellipse, line, and arrow.
7. Copy/paste rectangle, ellipse, arrow, and line cases inside Scribbleway.
8. Paste an Excalidraw clipboard JSON containing rectangle bound text and confirm it imports as attached text.

Expected: all smoke steps match the design.

- [ ] **Step 4: Commit verification-only updates**

No commit is required when Step 1-3 produce no file changes. If a packaging timestamp or generated file is intentionally tracked, commit it with:

```bash
git add scribbleway_0.1-1_amd64.deb
git commit -m "build: refresh debian package for attached text"
```

- [ ] **Step 5: Final status report**

Report:

- focused attached-text tests run and pass;
- full `shapesmodeltest` run and pass;
- full build run and pass;
- `.deb` package rebuilt;
- smoke test result, including any observed UI deviations.
