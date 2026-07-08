#include <QtTest>
#include <QClipboard>
#include <QGuiApplication>
#include "shapesmodel.h"
#include "overlaycontroller.h"
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>

class ShapesModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testAddShapeUndo();
    void testRemoveShapeUndo();
    void testClearUndo();
    void testSelectionNoHistory();
    void testCappedHistory();
    void testEditTransaction();
    void testOverlayControllerProperties();
    void testMoveShape();
    void testOverlayControllerCopyPaste();
    void testMultiSelection();
    void testBorderRadius();
    void testExcalidrawPasteCompatibility();
};

void ShapesModelTest::testAddShapeUndo()
{
    ShapesModel model;
    QCOMPARE(model.rowCount(), 0);

    QVariantMap shape;
    shape[QStringLiteral("type")] = QStringLiteral("rectangle");
    shape[QStringLiteral("x")] = 10;
    shape[QStringLiteral("y")] = 20;

    model.addShape(shape);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.shapes().first()[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));

    // Trigger undo
    model.undo();
    QCOMPARE(model.rowCount(), 0);
}

void ShapesModelTest::testRemoveShapeUndo()
{
    ShapesModel model;
    QVariantMap s1;
    s1[QStringLiteral("type")] = QStringLiteral("rectangle");
    s1[QStringLiteral("x")] = 10;
    model.addShape(s1);

    QVariantMap s2;
    s2[QStringLiteral("type")] = QStringLiteral("ellipse");
    s2[QStringLiteral("x")] = 20;
    model.addShape(s2);

    QCOMPARE(model.rowCount(), 2);

    model.removeShape(1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.shapes().first()[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));

    model.undo();
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.shapes().at(0)[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));
    QCOMPARE(model.shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
}

void ShapesModelTest::testClearUndo()
{
    ShapesModel model;
    QVariantMap s1;
    s1[QStringLiteral("type")] = QStringLiteral("rectangle");
    model.addShape(s1);
    QCOMPARE(model.rowCount(), 1);

    model.clear();
    QCOMPARE(model.rowCount(), 0);

    model.undo();
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.shapes().first()[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));
}

void ShapesModelTest::testSelectionNoHistory()
{
    ShapesModel model;
    QVariantMap s1;
    s1[QStringLiteral("type")] = QStringLiteral("rectangle");
    s1[QStringLiteral("selected")] = false;
    model.addShape(s1);

    QVariantMap s2;
    s2[QStringLiteral("type")] = QStringLiteral("ellipse");
    s2[QStringLiteral("selected")] = false;
    model.addShape(s2);

    // Update selected property on the second shape
    QVariantMap updateProps;
    updateProps[QStringLiteral("selected")] = true;
    model.updateShape(1, updateProps);

    // Call undo once. If selection update did not save history, it should undo the addition of shape 2.
    // So rowCount will become 1.
    model.undo();
    QCOMPARE(model.rowCount(), 1);
}

void ShapesModelTest::testCappedHistory()
{
    ShapesModel model;
    for (int i = 0; i < 55; ++i) {
        QVariantMap shape;
        shape[QStringLiteral("type")] = QStringLiteral("rectangle");
        shape[QStringLiteral("x")] = i;
        model.addShape(shape);
    }

    QCOMPARE(model.rowCount(), 55);

    // Undo 50 times.
    for (int i = 0; i < 50; ++i) {
        model.undo();
    }
    // We should reach state with 5 shapes.
    QCOMPARE(model.rowCount(), 5);

    // A 51st undo should be a no-op as the history stack is empty.
    model.undo();
    QCOMPARE(model.rowCount(), 5);
}

void ShapesModelTest::testEditTransaction()
{
    ShapesModel model;
    QVariantMap shape;
    shape[QStringLiteral("type")] = QStringLiteral("rectangle");
    shape[QStringLiteral("x")] = 10;
    shape[QStringLiteral("y")] = 20;
    model.addShape(shape);

    QCOMPARE(model.rowCount(), 1);

    // Start edit transaction
    model.beginEdit();

    // Update geometry coordinates repeatedly (simulating a drag operation)
    QVariantMap update1;
    update1[QStringLiteral("x")] = 11;
    model.updateShape(0, update1);

    QVariantMap update2;
    update2[QStringLiteral("x")] = 12;
    model.updateShape(0, update2);

    QVariantMap update3;
    update3[QStringLiteral("x")] = 15;
    model.updateShape(0, update3);

    // End transaction
    model.endEdit();

    // Total changes: 3 updates, but they were wrapped in beginEdit/endEdit.
    // So there should be exactly ONE history entry saved for the updates.
    // Let's verify by calling undo once:
    model.undo();

    // The state should revert to before the transaction started: shape.x should be 10.
    QCOMPARE(model.shapes().first()[QStringLiteral("x")].toInt(), 10);

    // Calling undo again should revert to before adding the shape: empty model.
    model.undo();
    QCOMPARE(model.rowCount(), 0);
}

void ShapesModelTest::testOverlayControllerProperties()
{
    OverlayController controller;
    
    // Check initial selection state
    QVariantMap selectionState = controller.getSelectionState();
    QCOMPARE(selectionState[QStringLiteral("hasSelection")].toBool(), false);
    QCOMPARE(selectionState[QStringLiteral("color")].toString(), QStringLiteral("#e63946")); // default color
    QCOMPARE(selectionState[QStringLiteral("strokeWidth")].toInt(), 2);
    QCOMPARE(selectionState[QStringLiteral("opacity")].toDouble(), 1.0);
    const QStringList families = QFontDatabase::families();
    QString expectedFont = families.contains(QStringLiteral("Cascadia Code"), Qt::CaseInsensitive)
        ? QStringLiteral("Cascadia Code")
        : QStringLiteral("monospace");
    QCOMPARE(selectionState[QStringLiteral("fontFamily")].toString(), expectedFont);
    QCOMPARE(selectionState[QStringLiteral("fontSize")].toInt(), 20);
    QCOMPARE(selectionState[QStringLiteral("selectedIndex")].toInt(), -1);

    // Call updateProperties to update defaults
    QVariantMap updateProps;
    updateProps[QStringLiteral("color")] = QStringLiteral("#0000ff");
    updateProps[QStringLiteral("strokeWidth")] = 5;
    updateProps[QStringLiteral("opacity")] = 0.5;
    updateProps[QStringLiteral("fontFamily")] = QStringLiteral("Arial");
    updateProps[QStringLiteral("fontSize")] = 20;

    controller.updateProperties(updateProps);

    // Check defaults updated
    QVariantMap selectionState2 = controller.getSelectionState();
    QCOMPARE(selectionState2[QStringLiteral("hasSelection")].toBool(), false);
    QCOMPARE(selectionState2[QStringLiteral("color")].toString(), QStringLiteral("#0000ff"));
    QCOMPARE(selectionState2[QStringLiteral("strokeWidth")].toInt(), 5);
    QCOMPARE(selectionState2[QStringLiteral("opacity")].toDouble(), 0.5);
    QCOMPARE(selectionState2[QStringLiteral("fontFamily")].toString(), QStringLiteral("Arial"));
    QCOMPARE(selectionState2[QStringLiteral("fontSize")].toInt(), 20);

    // Now add a shape, select it, and update properties on the selected shape
    QVariantMap shape;
    shape[QStringLiteral("type")] = QStringLiteral("rectangle");
    shape[QStringLiteral("color")] = QStringLiteral("#000000");
    shape[QStringLiteral("strokeWidth")] = 1;
    shape[QStringLiteral("opacity")] = 1.0;
    
    controller.addShape(shape); // This also selects it!
    QCOMPARE(controller.selectedIndex(), 0);

    QVariantMap selectionState3 = controller.getSelectionState();
    QCOMPARE(selectionState3[QStringLiteral("hasSelection")].toBool(), true);
    QCOMPARE(selectionState3[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));
    QCOMPARE(selectionState3[QStringLiteral("color")].toString(), QStringLiteral("#000000"));
    
    // Update shape properties via updateProperties
    QVariantMap updateShapeProps;
    updateShapeProps[QStringLiteral("color")] = QStringLiteral("#ff0000");
    updateShapeProps[QStringLiteral("strokeWidth")] = 8;
    
    controller.updateProperties(updateShapeProps);

    QVariantMap selectionState4 = controller.getSelectionState();
    QCOMPARE(selectionState4[QStringLiteral("color")].toString(), QStringLiteral("#ff0000"));
    QCOMPARE(selectionState4[QStringLiteral("strokeWidth")].toInt(), 8);
    
    // Check that shapesModel actually updated the shape
    QVariantMap updatedShape = controller.shapesModel()->shapes().first();
    QCOMPARE(updatedShape[QStringLiteral("color")].toString(), QStringLiteral("#ff0000"));
    QCOMPARE(updatedShape[QStringLiteral("strokeWidth")].toInt(), 8);
}

void ShapesModelTest::testMoveShape()
{
    ShapesModel model;
    QVariantMap s1;
    s1[QStringLiteral("type")] = QStringLiteral("rectangle");
    s1[QStringLiteral("x")] = 10;
    model.addShape(s1);

    QVariantMap s2;
    s2[QStringLiteral("type")] = QStringLiteral("ellipse");
    s2[QStringLiteral("x")] = 20;
    model.addShape(s2);

    QVariantMap s3;
    s3[QStringLiteral("type")] = QStringLiteral("line");
    s3[QStringLiteral("x")] = 30;
    model.addShape(s3);

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.shapes().at(0)[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));
    QCOMPARE(model.shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
    QCOMPARE(model.shapes().at(2)[QStringLiteral("type")].toString(), QStringLiteral("line"));

    // Test out of bounds
    QVERIFY(!model.moveShape(-1, 1));
    QVERIFY(!model.moveShape(0, 3));
    QVERIFY(!model.moveShape(3, 1));

    // Test no-op move
    QVERIFY(model.moveShape(1, 1));
    QCOMPARE(model.shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));

    // Test moving down: move s1 (index 0) to index 2
    QVERIFY(model.moveShape(0, 2));
    QCOMPARE(model.shapes().at(0)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
    QCOMPARE(model.shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("line"));
    QCOMPARE(model.shapes().at(2)[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));

    // Test undoing the move
    model.undo();
    QCOMPARE(model.shapes().at(0)[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));
    QCOMPARE(model.shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
    QCOMPARE(model.shapes().at(2)[QStringLiteral("type")].toString(), QStringLiteral("line"));

    // Test moving up: move s3 (index 2) to index 0
    QVERIFY(model.moveShape(2, 0));
    QCOMPARE(model.shapes().at(0)[QStringLiteral("type")].toString(), QStringLiteral("line"));
    QCOMPARE(model.shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));
    QCOMPARE(model.shapes().at(2)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));

    // Undo again
    model.undo();
    QCOMPARE(model.shapes().at(0)[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));
    QCOMPARE(model.shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
    QCOMPARE(model.shapes().at(2)[QStringLiteral("type")].toString(), QStringLiteral("line"));
}

void ShapesModelTest::testOverlayControllerCopyPaste()
{
    OverlayController controller;

    // Add a rectangle
    QVariantMap shape;
    shape[QStringLiteral("type")] = QStringLiteral("rectangle");
    shape[QStringLiteral("x")] = 10.0;
    shape[QStringLiteral("y")] = 20.0;
    shape[QStringLiteral("width")] = 100.0;
    shape[QStringLiteral("height")] = 50.0;
    shape[QStringLiteral("color")] = QStringLiteral("#ff0000");

    controller.addShape(shape); // This also selects it
    QCOMPARE(controller.selectedIndex(), 0);

    // Copy to clipboard
    controller.copySelected();

    // Verify clipboard contains the serialized shape
    QString clipboardText = QGuiApplication::clipboard()->text();
    QVERIFY(!clipboardText.isEmpty());
    QJsonDocument doc = QJsonDocument::fromJson(clipboardText.toUtf8());
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object().value(QStringLiteral("type")).toString(), QStringLiteral("excalidraw/clipboard"));
    QJsonArray elements = doc.object().value(QStringLiteral("elements")).toArray();
    QCOMPARE(elements.size(), 1);
    QJsonObject rectObj = elements.at(0).toObject();
    QCOMPARE(rectObj.value(QStringLiteral("type")).toString(), QStringLiteral("rectangle"));
    QCOMPARE(rectObj.value(QStringLiteral("x")).toDouble(), 10.0);
    QCOMPARE(rectObj.value(QStringLiteral("y")).toDouble(), 20.0);
    QCOMPARE(rectObj.value(QStringLiteral("width")).toDouble(), 100.0);
    QCOMPARE(rectObj.value(QStringLiteral("height")).toDouble(), 50.0);

    // Deselect shape
    controller.setSelectedIndex(-1);

    // Paste from clipboard. Pass local coordinates directly to avoid Wayland QCursor limitations.
    controller.pasteFromClipboard(150.0, 150.0);

    // Verify a new shape was pasted and selected
    QCOMPARE(controller.shapesModel()->rowCount(), 2);
    QCOMPARE(controller.selectedIndex(), 1);

    QVariantMap pastedShape = controller.shapesModel()->shapes().at(1);
    QCOMPARE(pastedShape[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));

    // Center of original shape: 10 + 100/2 = 60, 20 + 50/2 = 45.
    // Mouse position is at (150, 150), so delta to center is dx = 150 - 60 = 90, dy = 150 - 45 = 105.
    // New top-left: x = 10 + 90 = 100, y = 20 + 105 = 125.
    QCOMPARE(pastedShape[QStringLiteral("x")].toDouble(), 100.0);
    QCOMPARE(pastedShape[QStringLiteral("y")].toDouble(), 125.0);
    QCOMPARE(pastedShape[QStringLiteral("width")].toDouble(), 100.0);
    QCOMPARE(pastedShape[QStringLiteral("height")].toDouble(), 50.0);
}

void ShapesModelTest::testMultiSelection()
{
    OverlayController controller;

    // Add three shapes
    QVariantMap s1;
    s1[QStringLiteral("type")] = QStringLiteral("rectangle");
    s1[QStringLiteral("x")] = 10.0;
    s1[QStringLiteral("y")] = 10.0;
    s1[QStringLiteral("width")] = 50.0;
    s1[QStringLiteral("height")] = 50.0;
    s1[QStringLiteral("color")] = QStringLiteral("#ff0000");

    QVariantMap s2 = s1;
    s2[QStringLiteral("x")] = 100.0;
    s2[QStringLiteral("y")] = 100.0;

    QVariantMap s3 = s1;
    s3[QStringLiteral("x")] = 200.0;
    s3[QStringLiteral("y")] = 200.0;

    controller.addShape(s1);
    controller.addShape(s2);
    controller.addShape(s3);

    QCOMPARE(controller.shapesModel()->rowCount(), 3);

    // Initial selected index is 2 (since s3 was added last)
    QCOMPARE(controller.selectedIndex(), 2);
    QCOMPARE(controller.hasMultiSelection(), false);

    // Select shape 0 (clearing previous selection)
    controller.selectShape(0, false);
    QCOMPARE(controller.selectedIndex(), 0);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("selected")].toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("selected")].toBool(), false);
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("selected")].toBool(), false);

    // Shift-select shape 1
    controller.selectShape(1, true);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("selected")].toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("selected")].toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("selected")].toBool(), false);
    QCOMPARE(controller.hasMultiSelection(), true);

    // Test dragging the selected shapes group
    controller.beginEdit();
    controller.dragSelected(15.0, 25.0);
    controller.endEdit();

    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("x")].toDouble(), 25.0);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("y")].toDouble(), 35.0);
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("x")].toDouble(), 115.0);
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("y")].toDouble(), 125.0);
    // Shape 2 is not selected and should remain at (200.0, 200.0)
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("x")].toDouble(), 200.0);
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("y")].toDouble(), 200.0);

    // Test editing selected shapes together
    QVariantMap updateProps;
    updateProps[QStringLiteral("color")] = QStringLiteral("#0000ff");
    controller.updateProperties(updateProps);

    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("color")].toString(), QStringLiteral("#0000ff"));
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("color")].toString(), QStringLiteral("#0000ff"));
    // Shape 2 remains red
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("color")].toString(), QStringLiteral("#ff0000"));

    // Test drag selection helper
    // Rect from (0,0) to (150, 150) should select shape 0 and 1, but not 2.
    controller.beginDragSelection(false);
    controller.selectShapesInRect(0.0, 0.0, 150.0, 150.0, false);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("selected")].toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("selected")].toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("selected")].toBool(), false);

    // Test deleting selected shapes
    controller.deleteSelected();
    QCOMPARE(controller.shapesModel()->rowCount(), 1);
    // The only remaining shape should be shape 2
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("x")].toDouble(), 200.0);
}

void ShapesModelTest::testBorderRadius()
{
    OverlayController controller;

    // Check initial selection state has default border radius 8
    QVariantMap selectionState = controller.getSelectionState();
    QCOMPARE(selectionState[QStringLiteral("borderRadius")].toInt(), 8);

    // Update default border radius
    QVariantMap updateProps;
    updateProps[QStringLiteral("borderRadius")] = 12;
    controller.updateProperties(updateProps);

    QCOMPARE(controller.getSelectionState()[QStringLiteral("borderRadius")].toInt(), 12);
    QCOMPARE(controller.defaultBorderRadius(), 12);

    // Create a shape, select it, and check it inherits/updates border radius
    QVariantMap shape;
    shape[QStringLiteral("type")] = QStringLiteral("rectangle");
    shape[QStringLiteral("borderRadius")] = 5;
    controller.addShape(shape); // selects it automatically

    QCOMPARE(controller.selectedIndex(), 0);
    QCOMPARE(controller.getSelectionState()[QStringLiteral("borderRadius")].toInt(), 5);

    // Update properties on the selected shape
    QVariantMap updateShapeProps;
    updateShapeProps[QStringLiteral("borderRadius")] = 20;
    controller.updateProperties(updateShapeProps);

    QCOMPARE(controller.getSelectionState()[QStringLiteral("borderRadius")].toInt(), 20);
    QCOMPARE(controller.shapesModel()->shapes().first()[QStringLiteral("borderRadius")].toInt(), 20);
}

void ShapesModelTest::testExcalidrawPasteCompatibility()
{
    OverlayController controller;

    // Build the Excalidraw clipboard JSON payload
    QJsonObject clipboardObj;
    clipboardObj.insert(QStringLiteral("type"), QStringLiteral("excalidraw/clipboard"));

    QJsonArray elements;

    // 1. Rectangle element with roundness and extraneous fields
    QJsonObject rectObj;
    rectObj.insert(QStringLiteral("type"), QStringLiteral("rectangle"));
    rectObj.insert(QStringLiteral("x"), 10.0);
    rectObj.insert(QStringLiteral("y"), 20.0);
    rectObj.insert(QStringLiteral("width"), 100.0);
    rectObj.insert(QStringLiteral("height"), 50.0);
    rectObj.insert(QStringLiteral("strokeColor"), QStringLiteral("#ff0000"));
    rectObj.insert(QStringLiteral("strokeWidth"), 2.0);
    rectObj.insert(QStringLiteral("opacity"), 80.0);
    rectObj.insert(QStringLiteral("seed"), 123456);
    rectObj.insert(QStringLiteral("version"), 1);
    rectObj.insert(QStringLiteral("roughness"), 1);
    rectObj.insert(QStringLiteral("angle"), 0.5);
    rectObj.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
    rectObj.insert(QStringLiteral("strokeStyle"), QStringLiteral("dashed"));
    
    QJsonObject roundnessObj;
    roundnessObj.insert(QStringLiteral("type"), 3);
    roundnessObj.insert(QStringLiteral("value"), 15);
    rectObj.insert(QStringLiteral("roundness"), roundnessObj);
    
    elements.append(rectObj);

    // 2. Text element with font family/size and extraneous fields
    QJsonObject textObj;
    textObj.insert(QStringLiteral("type"), QStringLiteral("text"));
    textObj.insert(QStringLiteral("x"), 50.0);
    textObj.insert(QStringLiteral("y"), 60.0);
    textObj.insert(QStringLiteral("width"), 80.0);
    textObj.insert(QStringLiteral("height"), 30.0);
    textObj.insert(QStringLiteral("strokeColor"), QStringLiteral("#ffffff"));
    textObj.insert(QStringLiteral("strokeWidth"), 1.0);
    textObj.insert(QStringLiteral("opacity"), 100.0);
    textObj.insert(QStringLiteral("text"), QStringLiteral("Hello World"));
    textObj.insert(QStringLiteral("fontSize"), 24);
    textObj.insert(QStringLiteral("fontFamily"), 3);
    textObj.insert(QStringLiteral("seed"), 789012);
    textObj.insert(QStringLiteral("version"), 2);
    textObj.insert(QStringLiteral("roughness"), 0);
    textObj.insert(QStringLiteral("angle"), 0.0);
    textObj.insert(QStringLiteral("fillStyle"), QStringLiteral("hachure"));
    textObj.insert(QStringLiteral("strokeStyle"), QStringLiteral("solid"));
    
    elements.append(textObj);

    // 3. Freedraw (freehand) element with relative coordinates and extraneous fields
    QJsonObject freeObj;
    freeObj.insert(QStringLiteral("type"), QStringLiteral("freedraw"));
    freeObj.insert(QStringLiteral("x"), 200.0);
    freeObj.insert(QStringLiteral("y"), 100.0);
    freeObj.insert(QStringLiteral("strokeColor"), QStringLiteral("#0000ff"));
    freeObj.insert(QStringLiteral("strokeWidth"), 3.0);
    freeObj.insert(QStringLiteral("opacity"), 90.0);
    freeObj.insert(QStringLiteral("seed"), 345678);
    freeObj.insert(QStringLiteral("version"), 3);
    freeObj.insert(QStringLiteral("roughness"), 2);
    freeObj.insert(QStringLiteral("angle"), 0.0);
    freeObj.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
    freeObj.insert(QStringLiteral("strokeStyle"), QStringLiteral("solid"));

    QJsonArray pts;
    QJsonArray p1; p1.append(0.0); p1.append(0.0);
    QJsonArray p2; p2.append(10.0); p2.append(20.0);
    QJsonArray p3; p3.append(50.0); p3.append(50.0);
    pts.append(p1);
    pts.append(p2);
    pts.append(p3);
    freeObj.insert(QStringLiteral("points"), pts);

    elements.append(freeObj);

    clipboardObj.insert(QStringLiteral("elements"), elements);

    QJsonDocument doc(clipboardObj);
    QGuiApplication::clipboard()->setText(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));

    // Paste at the center of the bounding box (130.0, 85.0) to have zero shift (dx=0, dy=0)
    controller.pasteFromClipboard(130.0, 85.0);

    QCOMPARE(controller.shapesModel()->rowCount(), 3);

    // Get the pasted shapes from the model
    QVariantMap pastedRect = controller.shapesModel()->shapes().at(0);
    QVariantMap pastedText = controller.shapesModel()->shapes().at(1);
    QVariantMap pastedFree = controller.shapesModel()->shapes().at(2);

    // Verify Rectangle properties
    QCOMPARE(pastedRect[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));
    QCOMPARE(pastedRect[QStringLiteral("x")].toDouble(), 10.0);
    QCOMPARE(pastedRect[QStringLiteral("y")].toDouble(), 20.0);
    QCOMPARE(pastedRect[QStringLiteral("width")].toDouble(), 100.0);
    QCOMPARE(pastedRect[QStringLiteral("height")].toDouble(), 50.0);
    QCOMPARE(pastedRect[QStringLiteral("color")].toString(), QStringLiteral("#ff0000"));
    QCOMPARE(pastedRect[QStringLiteral("strokeWidth")].toDouble(), 2.0);
    QCOMPARE(pastedRect[QStringLiteral("opacity")].toDouble(), 0.8);
    QCOMPARE(pastedRect[QStringLiteral("borderRadius")].toInt(), 15);

    // Verify Text properties
    QCOMPARE(pastedText[QStringLiteral("type")].toString(), QStringLiteral("text"));
    QCOMPARE(pastedText[QStringLiteral("x")].toDouble(), 50.0);
    QCOMPARE(pastedText[QStringLiteral("y")].toDouble(), 60.0);
    QCOMPARE(pastedText[QStringLiteral("width")].toDouble(), 80.0);
    QCOMPARE(pastedText[QStringLiteral("height")].toDouble(), 30.0);
    QCOMPARE(pastedText[QStringLiteral("color")].toString(), QStringLiteral("#e63946"));
    QCOMPARE(pastedText[QStringLiteral("strokeWidth")].toDouble(), 1.0);
    QCOMPARE(pastedText[QStringLiteral("opacity")].toDouble(), 1.0);
    QCOMPARE(pastedText[QStringLiteral("text")].toString(), QStringLiteral("Hello World"));
    QCOMPARE(pastedText[QStringLiteral("fontSize")].toInt(), 24);
    QCOMPARE(pastedText[QStringLiteral("fontFamily")].toString(), QStringLiteral("Cascadia Code"));

    // Verify Freehand properties and absolute coordinates
    QCOMPARE(pastedFree[QStringLiteral("type")].toString(), QStringLiteral("freehand"));
    QCOMPARE(pastedFree[QStringLiteral("color")].toString(), QStringLiteral("#0000ff"));
    QCOMPARE(pastedFree[QStringLiteral("strokeWidth")].toDouble(), 3.0);
    QCOMPARE(pastedFree[QStringLiteral("opacity")].toDouble(), 0.9);

    QVariantList points = pastedFree[QStringLiteral("points")].toList();
    QCOMPARE(points.size(), 3);
    QCOMPARE(points.at(0).toPointF().x(), 200.0);
    QCOMPARE(points.at(0).toPointF().y(), 100.0);
    QCOMPARE(points.at(1).toPointF().x(), 210.0);
    QCOMPARE(points.at(1).toPointF().y(), 120.0);
    QCOMPARE(points.at(2).toPointF().x(), 250.0);
    QCOMPARE(points.at(2).toPointF().y(), 150.0);

    // Verify that non-relevant/extraneous properties are completely stripped and not loaded
    const QStringList extraneousKeys = {
        QStringLiteral("seed"),
        QStringLiteral("version"),
        QStringLiteral("roughness"),
        QStringLiteral("angle"),
        QStringLiteral("fillStyle"),
        QStringLiteral("strokeStyle"),
        QStringLiteral("updated"),
        QStringLiteral("versionNonce"),
        QStringLiteral("isDeleted"),
        QStringLiteral("roundness")
    };

    for (const QVariantMap &shape : {pastedRect, pastedText, pastedFree}) {
        for (const QString &key : extraneousKeys) {
            QVERIFY(!shape.contains(key));
        }
    }
}


QTEST_MAIN(ShapesModelTest)
#include "shapesmodeltest.moc"
