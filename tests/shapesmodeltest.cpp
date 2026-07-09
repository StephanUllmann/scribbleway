#include <QtTest>
#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QSignalSpy>
#include <QAction>
#include <QKeySequence>
#include <KGlobalAccel>
#include "shapesmodel.h"
#include "overlaycontroller.h"
#include "appletbackend.h"
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>

class ShapesModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
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
    void testZOrder();
    void testShapeLock();
    void testToolModeTransitions();
    void testPropertiesDefaultsUpdates();
    void testMultiSelectionDragDelete();
    void testExcalidrawSchemaEdgeCases();
    void testAppletBackendIntegration();
    void testReworkedShortcutsSlots();
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
    QCOMPARE(selectionState[QStringLiteral("fontFamily")].toString(), QStringLiteral("monospace"));
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


void ShapesModelTest::initTestCase()
{
    // Clean up any stray scribbleway-overlay instances
    QProcess::execute(QStringLiteral("pkill"), {QStringLiteral("-f"), QStringLiteral("scribbleway-overlay")});
    QTest::qWait(100);
}

void ShapesModelTest::cleanupTestCase()
{
}

void ShapesModelTest::testZOrder()
{
    OverlayController controller;
    
    QVariantMap s1;
    s1[QStringLiteral("type")] = QStringLiteral("rectangle");
    QVariantMap s2;
    s2[QStringLiteral("type")] = QStringLiteral("ellipse");
    QVariantMap s3;
    s3[QStringLiteral("type")] = QStringLiteral("line");
    
    controller.addShape(s1);
    controller.addShape(s2);
    controller.addShape(s3);
    
    QCOMPARE(controller.shapesModel()->rowCount(), 3);
    QCOMPARE(controller.selectedIndex(), 2);
    
    // Top boundary constraint
    controller.raiseSelected();
    QCOMPARE(controller.selectedIndex(), 2);
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("type")].toString(), QStringLiteral("line"));
    
    // Select shape 1
    controller.selectShape(1, false);
    QCOMPARE(controller.selectedIndex(), 1);
    
    // Raise shape 1
    controller.raiseSelected();
    QCOMPARE(controller.selectedIndex(), 2);
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("line"));
    
    // Raising it again should be no-op
    controller.raiseSelected();
    QCOMPARE(controller.selectedIndex(), 2);
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
    
    // Lower shape 2
    controller.lowerSelected();
    QCOMPARE(controller.selectedIndex(), 1);
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("type")].toString(), QStringLiteral("line"));
    
    // Lower it again
    controller.lowerSelected();
    QCOMPARE(controller.selectedIndex(), 0);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));
    
    // Lower it again should be no-op
    controller.lowerSelected();
    QCOMPARE(controller.selectedIndex(), 0);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
    
    // Test undoing the last lower operation
    controller.undo();
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("type")].toString(), QStringLiteral("rectangle"));
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("type")].toString(), QStringLiteral("ellipse"));
    QCOMPARE(controller.shapesModel()->shapes().at(2)[QStringLiteral("type")].toString(), QStringLiteral("line"));
    QCOMPARE(controller.selectedIndex(), -1);
}

void ShapesModelTest::testShapeLock()
{
    OverlayController controller;
    
    QVariantMap shape;
    shape[QStringLiteral("type")] = QStringLiteral("rectangle");
    controller.addShape(shape);
    
    QCOMPARE(controller.selectedIndex(), 0);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("locked")].toBool(), false);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("selected")].toBool(), true);
    
    // Lock shape
    controller.setShapeLocked(0, true);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("locked")].toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("selected")].toBool(), false);
    QCOMPARE(controller.selectedIndex(), -1);
    
    // Undo locking
    controller.undo();
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("locked")].toBool(), false);
    
    // Select again
    controller.selectShape(0, false);
    QCOMPARE(controller.selectedIndex(), 0);
    
    // Toggle lock
    controller.toggleLock();
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("locked")].toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("selected")].toBool(), false);
    QCOMPARE(controller.selectedIndex(), -1);
    
    // Unlock via toggleLock
    controller.selectShape(0, false);
    controller.toggleLock();
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("locked")].toBool(), false);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("selected")].toBool(), true);
    
    // Multi-selection locking
    QVariantMap shape2;
    shape2[QStringLiteral("type")] = QStringLiteral("ellipse");
    controller.addShape(shape2);
    
    controller.selectShape(0, false);
    controller.selectShape(1, true);
    QCOMPARE(controller.hasMultiSelection(), true);
    
    controller.toggleLock();
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("locked")].toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("locked")].toBool(), true);
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("selected")].toBool(), false);
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("selected")].toBool(), false);
    QCOMPARE(controller.selectedIndex(), -1);
    
    controller.selectShape(0, false);
    controller.selectShape(1, true);
    controller.toggleLock();
    QCOMPARE(controller.shapesModel()->shapes().at(0)[QStringLiteral("locked")].toBool(), false);
    QCOMPARE(controller.shapesModel()->shapes().at(1)[QStringLiteral("locked")].toBool(), false);
}

void ShapesModelTest::testToolModeTransitions()
{
    OverlayController controller;
    
    QCOMPARE(controller.currentMode(), QStringLiteral("passthrough"));
    QCOMPARE(controller.activeTool(), QStringLiteral("freehand"));
    
    QVariantMap shape;
    shape[QStringLiteral("type")] = QStringLiteral("rectangle");
    controller.addShape(shape);
    QCOMPARE(controller.selectedIndex(), 0);
    
    controller.setActiveTool(QStringLiteral("rectangle"));
    QCOMPARE(controller.activeTool(), QStringLiteral("rectangle"));
    QCOMPARE(controller.currentMode(), QStringLiteral("draw"));
    QCOMPARE(controller.selectedIndex(), -1);
    
    controller.setActiveTool(QString());
    QCOMPARE(controller.activeTool(), QString());
    QCOMPARE(controller.currentMode(), QStringLiteral("passthrough"));
    
    controller.setActiveTool(QStringLiteral("ellipse"));
    controller.enterSelectMode();
    QCOMPARE(controller.currentMode(), QStringLiteral("select"));
    QCOMPARE(controller.activeTool(), QString());
    
    controller.setActiveTool(QStringLiteral("ellipse"));
    controller.enterPassthroughMode();
    QCOMPARE(controller.currentMode(), QStringLiteral("passthrough"));
    QCOMPARE(controller.activeTool(), QString());
}

void ShapesModelTest::testPropertiesDefaultsUpdates()
{
    OverlayController controller;
    
    QVariantMap defaults;
    defaults[QStringLiteral("color")] = QStringLiteral("#00ff00");
    defaults[QStringLiteral("strokeWidth")] = 6;
    defaults[QStringLiteral("opacity")] = 0.75;
    defaults[QStringLiteral("fontFamily")] = QStringLiteral("serif");
    defaults[QStringLiteral("fontSize")] = 25;
    defaults[QStringLiteral("borderRadius")] = 15;
    
    controller.updateProperties(defaults);
    
    QCOMPARE(controller.defaultColor(), QStringLiteral("#00ff00"));
    QCOMPARE(controller.defaultStrokeWidth(), 6);
    QCOMPARE(controller.defaultOpacity(), 0.75);
    QCOMPARE(controller.defaultFontFamily(), QStringLiteral("serif"));
    QCOMPARE(controller.defaultFontSize(), 25);
    QCOMPARE(controller.defaultBorderRadius(), 15);
    
    QVariantMap s1; s1[QStringLiteral("type")] = QStringLiteral("rectangle");
    QVariantMap s2; s2[QStringLiteral("type")] = QStringLiteral("ellipse");
    QVariantMap s3; s3[QStringLiteral("type")] = QStringLiteral("text");
    controller.addShape(s1);
    controller.addShape(s2);
    controller.addShape(s3);
    
    controller.selectShape(0, false);
    controller.selectShape(1, true);
    
    QVariantMap updates;
    updates[QStringLiteral("color")] = QStringLiteral("#ff00ff");
    updates[QStringLiteral("strokeWidth")] = 10;
    updates[QStringLiteral("opacity")] = 0.4;
    updates[QStringLiteral("borderRadius")] = 30;
    
    controller.updateProperties(updates);
    
    QVariantMap shape0 = controller.shapesModel()->shapes().at(0);
    QVariantMap shape1 = controller.shapesModel()->shapes().at(1);
    QVariantMap shape2 = controller.shapesModel()->shapes().at(2);
    
    QCOMPARE(shape0[QStringLiteral("color")].toString(), QStringLiteral("#ff00ff"));
    QCOMPARE(shape0[QStringLiteral("strokeWidth")].toInt(), 10);
    QCOMPARE(shape0[QStringLiteral("opacity")].toDouble(), 0.4);
    QCOMPARE(shape0[QStringLiteral("borderRadius")].toInt(), 30);
    
    QCOMPARE(shape1[QStringLiteral("color")].toString(), QStringLiteral("#ff00ff"));
    QCOMPARE(shape1[QStringLiteral("strokeWidth")].toInt(), 10);
    QCOMPARE(shape1[QStringLiteral("opacity")].toDouble(), 0.4);
    QCOMPARE(shape1[QStringLiteral("borderRadius")].toInt(), 30);
    
    QVERIFY(shape2[QStringLiteral("color")].toString() != QStringLiteral("#ff00ff"));
    
    QCOMPARE(controller.defaultColor(), QStringLiteral("#ff00ff"));
    QCOMPARE(controller.defaultStrokeWidth(), 10);
    QCOMPARE(controller.defaultOpacity(), 0.4);
    QCOMPARE(controller.defaultBorderRadius(), 30);
}

void ShapesModelTest::testMultiSelectionDragDelete()
{
    OverlayController controller;
    
    QVariantMap rect;
    rect[QStringLiteral("type")] = QStringLiteral("rectangle");
    rect[QStringLiteral("x")] = 10.0; rect[QStringLiteral("y")] = 15.0;
    
    QVariantMap ellipse;
    ellipse[QStringLiteral("type")] = QStringLiteral("ellipse");
    ellipse[QStringLiteral("x")] = 20.0; ellipse[QStringLiteral("y")] = 25.0;
    
    QVariantMap text;
    text[QStringLiteral("type")] = QStringLiteral("text");
    text[QStringLiteral("x")] = 30.0; text[QStringLiteral("y")] = 35.0;
    
    QVariantMap line;
    line[QStringLiteral("type")] = QStringLiteral("line");
    line[QStringLiteral("fromX")] = 5.0; line[QStringLiteral("fromY")] = 10.0;
    line[QStringLiteral("toX")] = 50.0; line[QStringLiteral("toY")] = 60.0;
    
    QVariantMap arrow;
    arrow[QStringLiteral("type")] = QStringLiteral("arrow");
    arrow[QStringLiteral("fromX")] = 15.0; arrow[QStringLiteral("fromY")] = 20.0;
    arrow[QStringLiteral("toX")] = 70.0; arrow[QStringLiteral("toY")] = 80.0;
    
    QVariantMap freehand;
    freehand[QStringLiteral("type")] = QStringLiteral("freehand");
    QVariantList pts;
    pts.append(QPointF(0.0, 1.0));
    pts.append(QPointF(10.0, 12.0));
    freehand[QStringLiteral("points")] = pts;
    
    QVariantMap freehandMap;
    freehandMap[QStringLiteral("type")] = QStringLiteral("freehand");
    QVariantList ptsMap;
    ptsMap.append(QVariantMap{{QStringLiteral("x"), 100.0}, {QStringLiteral("y"), 150.0}});
    ptsMap.append(QVariantMap{{QStringLiteral("x"), 110.0}, {QStringLiteral("y"), 165.0}});
    freehandMap[QStringLiteral("points")] = ptsMap;
    
    controller.addShape(rect);
    controller.addShape(ellipse);
    controller.addShape(text);
    controller.addShape(line);
    controller.addShape(arrow);
    controller.addShape(freehand);
    controller.addShape(freehandMap);
    
    QCOMPARE(controller.shapesModel()->rowCount(), 7);
    
    controller.setSelectedIndex(-1);
    for (int i = 0; i < 7; ++i) {
        controller.selectShape(i, true);
    }
    QCOMPARE(controller.hasMultiSelection(), true);
    
    controller.beginEdit();
    controller.dragSelected(10.0, 20.0);
    controller.endEdit();
    
    QVariantMap r_rect = controller.shapesModel()->shapes().at(0);
    QCOMPARE(r_rect[QStringLiteral("x")].toDouble(), 20.0);
    QCOMPARE(r_rect[QStringLiteral("y")].toDouble(), 35.0);
    
    QVariantMap r_ellipse = controller.shapesModel()->shapes().at(1);
    QCOMPARE(r_ellipse[QStringLiteral("x")].toDouble(), 30.0);
    QCOMPARE(r_ellipse[QStringLiteral("y")].toDouble(), 45.0);
    
    QVariantMap r_text = controller.shapesModel()->shapes().at(2);
    QCOMPARE(r_text[QStringLiteral("x")].toDouble(), 40.0);
    QCOMPARE(r_text[QStringLiteral("y")].toDouble(), 55.0);
    
    QVariantMap r_line = controller.shapesModel()->shapes().at(3);
    QCOMPARE(r_line[QStringLiteral("fromX")].toDouble(), 15.0);
    QCOMPARE(r_line[QStringLiteral("fromY")].toDouble(), 30.0);
    QCOMPARE(r_line[QStringLiteral("toX")].toDouble(), 60.0);
    QCOMPARE(r_line[QStringLiteral("toY")].toDouble(), 80.0);
    
    QVariantMap r_arrow = controller.shapesModel()->shapes().at(4);
    QCOMPARE(r_arrow[QStringLiteral("fromX")].toDouble(), 25.0);
    QCOMPARE(r_arrow[QStringLiteral("fromY")].toDouble(), 40.0);
    QCOMPARE(r_arrow[QStringLiteral("toX")].toDouble(), 80.0);
    QCOMPARE(r_arrow[QStringLiteral("toY")].toDouble(), 100.0);
    
    QVariantMap r_freehand = controller.shapesModel()->shapes().at(5);
    QVariantList r_pts = r_freehand[QStringLiteral("points")].toList();
    QCOMPARE(r_pts.at(0).toPointF().x(), 10.0);
    QCOMPARE(r_pts.at(0).toPointF().y(), 21.0);
    QCOMPARE(r_pts.at(1).toPointF().x(), 20.0);
    QCOMPARE(r_pts.at(1).toPointF().y(), 32.0);
    
    QVariantMap r_freehandMap = controller.shapesModel()->shapes().at(6);
    QVariantList r_ptsMap = r_freehandMap[QStringLiteral("points")].toList();
    QCOMPARE(r_ptsMap.at(0).toPointF().x(), 110.0);
    QCOMPARE(r_ptsMap.at(0).toPointF().y(), 170.0);
    QCOMPARE(r_ptsMap.at(1).toPointF().x(), 120.0);
    QCOMPARE(r_ptsMap.at(1).toPointF().y(), 185.0);
    
    controller.deleteSelected();
    QCOMPARE(controller.shapesModel()->rowCount(), 0);
}

void ShapesModelTest::testExcalidrawSchemaEdgeCases()
{
    OverlayController controller;
    
    // 1. Opacity scaling (0..1 vs 0..100)
    QJsonObject excalObj;
    excalObj.insert(QStringLiteral("type"), QStringLiteral("rectangle"));
    excalObj.insert(QStringLiteral("opacity"), 75.0);
    excalObj.insert(QStringLiteral("strokeWidth"), 2.0);
    excalObj.insert(QStringLiteral("strokeColor"), QStringLiteral("#e63946"));
    
    QJsonObject clipboardObj;
    clipboardObj.insert(QStringLiteral("type"), QStringLiteral("excalidraw/clipboard"));
    QJsonArray elements;
    elements.append(excalObj);
    clipboardObj.insert(QStringLiteral("elements"), elements);
    
    QJsonDocument doc(clipboardObj);
    QGuiApplication::clipboard()->setText(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    
    controller.pasteFromClipboard(0.0, 0.0);
    QCOMPARE(controller.shapesModel()->rowCount(), 1);
    QVariantMap pastedRect = controller.shapesModel()->shapes().at(0);
    QCOMPARE(pastedRect[QStringLiteral("opacity")].toDouble(), 0.75);
    
    controller.copySelected();
    QString cbText = QGuiApplication::clipboard()->text();
    QJsonDocument docOut = QJsonDocument::fromJson(cbText.toUtf8());
    QJsonObject rectObj = docOut.object().value(QStringLiteral("elements")).toArray().at(0).toObject();
    QCOMPARE(rectObj.value(QStringLiteral("opacity")).toDouble(), 75.0);
    
    // 2. Roundness translation
    controller.clear();
    
    QJsonObject rectWithRoundness;
    rectWithRoundness.insert(QStringLiteral("type"), QStringLiteral("rectangle"));
    rectWithRoundness.insert(QStringLiteral("strokeWidth"), 2.0);
    
    QJsonObject roundnessObj;
    roundnessObj.insert(QStringLiteral("type"), 3);
    roundnessObj.insert(QStringLiteral("value"), 18);
    rectWithRoundness.insert(QStringLiteral("roundness"), roundnessObj);
    
    QJsonArray elements2;
    elements2.append(rectWithRoundness);
    QJsonObject clipboardObj2;
    clipboardObj2.insert(QStringLiteral("type"), QStringLiteral("excalidraw/clipboard"));
    clipboardObj2.insert(QStringLiteral("elements"), elements2);
    
    QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(clipboardObj2).toJson(QJsonDocument::Compact)));
    
    controller.pasteFromClipboard(0.0, 0.0);
    QVariantMap pastedRect2 = controller.shapesModel()->shapes().at(0);
    QCOMPARE(pastedRect2[QStringLiteral("borderRadius")].toInt(), 18);
    
    controller.clear();
    rectWithRoundness.insert(QStringLiteral("roundness"), QJsonValue::Null);
    elements2.removeAt(0);
    elements2.append(rectWithRoundness);
    clipboardObj2.insert(QStringLiteral("elements"), elements2);
    QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(clipboardObj2).toJson(QJsonDocument::Compact)));
    
    controller.pasteFromClipboard(0.0, 0.0);
    QVariantMap pastedRect3 = controller.shapesModel()->shapes().at(0);
    QCOMPARE(pastedRect3[QStringLiteral("borderRadius")].toInt(), 0);
    
    controller.updateProperties({{QStringLiteral("borderRadius"), 22}});
    controller.copySelected();
    QString cbText3 = QGuiApplication::clipboard()->text();
    QJsonObject rectObj3 = QJsonDocument::fromJson(cbText3.toUtf8()).object().value(QStringLiteral("elements")).toArray().at(0).toObject();
    QJsonObject ro = rectObj3.value(QStringLiteral("roundness")).toObject();
    QCOMPARE(ro.value(QStringLiteral("type")).toInt(), 3);
    QCOMPARE(ro.value(QStringLiteral("value")).toInt(), 22);
    
    controller.updateProperties({{QStringLiteral("borderRadius"), 0}});
    controller.copySelected();
    QString cbText4 = QGuiApplication::clipboard()->text();
    QJsonObject rectObj4 = QJsonDocument::fromJson(cbText4.toUtf8()).object().value(QStringLiteral("elements")).toArray().at(0).toObject();
    QVERIFY(rectObj4.value(QStringLiteral("roundness")).isNull());
}

void ShapesModelTest::testAppletBackendIntegration()
{
    OverlayController controller;
    
    QVERIFY(QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.scribbleway")));
    QVERIFY(QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/Overlay"),
        &controller,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
    ));
    
    AppletBackend backend;
    QTest::qWait(150);
    
    QVERIFY(backend.overlayConnected());
    
    QCOMPARE(backend.hasSelection(), false);
    QCOMPARE(backend.selectedColor(), QStringLiteral("#e63946"));
    QCOMPARE(backend.selectedStrokeWidth(), 2);
    QCOMPARE(backend.selectedOpacity(), 1.0);
    QCOMPARE(backend.currentMode(), QStringLiteral("passthrough"));
    QCOMPARE(backend.activeTool(), QStringLiteral("freehand"));
    
    QVariantMap update;
    update[QStringLiteral("color")] = QStringLiteral("#00ffff");
    update[QStringLiteral("strokeWidth")] = 5;
    update[QStringLiteral("opacity")] = 0.5;
    controller.updateProperties(update);
    QTest::qWait(50);
    
    QCOMPARE(backend.selectedColor(), QStringLiteral("#00ffff"));
    QCOMPARE(backend.selectedStrokeWidth(), 5);
    QCOMPARE(backend.selectedOpacity(), 0.5);
    
    controller.setActiveTool(QStringLiteral("ellipse"));
    QTest::qWait(50);
    QCOMPARE(backend.activeTool(), QStringLiteral("ellipse"));
    QCOMPARE(backend.currentMode(), QStringLiteral("draw"));
    
    backend.setTargetScreen(QStringLiteral("CustomScreen"));
    QTest::qWait(50);
    QCOMPARE(backend.targetScreen(), QStringLiteral("CustomScreen"));
    
    QVERIFY(!backend.screenNames().isEmpty());
    
    backend.setTool(QStringLiteral("rectangle"));
    QTest::qWait(50);
    QCOMPARE(controller.activeTool(), QStringLiteral("rectangle"));
    QCOMPARE(controller.currentMode(), QStringLiteral("draw"));
    
    backend.setColor(QStringLiteral("#112233"));
    backend.setStrokeWidth(8);
    backend.setOpacity(0.25);
    QTest::qWait(50);
    QCOMPARE(controller.defaultColor(), QStringLiteral("#112233"));
    QCOMPARE(controller.defaultStrokeWidth(), 8);
    QCOMPARE(controller.defaultOpacity(), 0.25);
    
    QVariantMap shape1;
    shape1[QStringLiteral("type")] = QStringLiteral("rectangle");
    controller.addShape(shape1);
    QTest::qWait(50);
    
    QVERIFY(backend.hasSelection());
    QCOMPARE(backend.selectedShapeIndex(), 0);
    
    QVariantMap shape2;
    shape2[QStringLiteral("type")] = QStringLiteral("ellipse");
    controller.addShape(shape2);
    QTest::qWait(50);
    
    QCOMPARE(backend.selectedShapeIndex(), 1);
    
    backend.lowerSelected();
    QTest::qWait(50);
    QCOMPARE(controller.selectedIndex(), 0);
    QCOMPARE(backend.selectedShapeIndex(), 0);
    
    backend.raiseSelected();
    QTest::qWait(50);
    QCOMPARE(controller.selectedIndex(), 1);
    QCOMPARE(backend.selectedShapeIndex(), 1);
    
    backend.toggleLock();
    QTest::qWait(50);
    QVERIFY(controller.shapesModel()->shapes().at(1)[QStringLiteral("locked")].toBool());
    QCOMPARE(backend.hasSelection(), false);
    
    backend.setShapeLocked(1, false);
    QTest::qWait(50);
    QVERIFY(!controller.shapesModel()->shapes().at(1)[QStringLiteral("locked")].toBool());
    
    backend.selectShape(1);
    QTest::qWait(50);
    QCOMPARE(controller.selectedIndex(), 1);
    QVERIFY(backend.hasSelection());
    
    backend.deleteSelected();
    QTest::qWait(50);
    QCOMPARE(controller.shapesModel()->rowCount(), 1);
    
    backend.deleteShape(0);
    QTest::qWait(50);
    QCOMPARE(controller.shapesModel()->rowCount(), 0);
    
    controller.addShape(shape1);
    QTest::qWait(50);
    QCOMPARE(controller.shapesModel()->rowCount(), 1);
    backend.clear();
    QTest::qWait(50);
    QCOMPARE(controller.shapesModel()->rowCount(), 0);
    
    controller.addShape(shape1);
    QTest::qWait(50);
    QCOMPARE(controller.shapesModel()->rowCount(), 1);
    backend.undo();
    QTest::qWait(50);
    QCOMPARE(controller.shapesModel()->rowCount(), 0);
    
    backend.enterSelectMode();
    QTest::qWait(50);
    QCOMPARE(controller.currentMode(), QStringLiteral("select"));
    
    backend.enterPassthroughMode();
    QTest::qWait(50);
    QCOMPARE(controller.currentMode(), QStringLiteral("passthrough"));
    
    QAction action(&controller);
    action.setObjectName(QStringLiteral("action_test"));
    controller.registerAction(&action, QStringLiteral("action_test"), QStringLiteral("Test Action"));
    backend.changeShortcut(QStringLiteral("action_test"), QStringLiteral("Ctrl+Shift+D"));
    QTest::qWait(50);
    QList<QKeySequence> seqs = KGlobalAccel::self()->shortcut(&action);
    QVERIFY(!seqs.isEmpty());
    QCOMPARE(seqs.first().toString(), QStringLiteral("Ctrl+Shift+D"));
    
    QString formatted = backend.formatKeySequence(Qt::Key_K, Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(formatted, QStringLiteral("Ctrl+Shift+K"));
    
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/Overlay"));
    QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.kde.scribbleway"));
}

void ShapesModelTest::testReworkedShortcutsSlots()
{
    // Clear settings to prevent state leakage from previous failed test runs
    QSettings preClean(QStringLiteral("scribbleway"), QStringLiteral("shortcuts"));
    preClean.clear();

    OverlayController controller;
    QAction dummyAction(&controller);
    dummyAction.setObjectName(QStringLiteral("action_dummy"));
    controller.registerAction(&dummyAction, QStringLiteral("action_dummy"), QStringLiteral("Dummy Action"));

    // Toggle tool to "arrow". Since current active tool is "freehand", it should activate "arrow".
    controller.toggleTool(QStringLiteral("arrow"));
    QCOMPARE(controller.activeTool(), QStringLiteral("arrow"));
    QCOMPARE(controller.currentMode(), QStringLiteral("draw"));

    // Toggle "arrow" again. Since it is active, it should return to select mode.
    controller.toggleTool(QStringLiteral("arrow"));
    QCOMPARE(controller.activeTool(), QStringLiteral(""));
    QCOMPARE(controller.currentMode(), QStringLiteral("select"));

    // Toggle "rectangle". Since active is "", it should activate "rectangle".
    controller.toggleTool(QStringLiteral("rectangle"));
    QCOMPARE(controller.activeTool(), QStringLiteral("rectangle"));
    QCOMPARE(controller.currentMode(), QStringLiteral("draw"));

    // Toggle "arrow". Since "rectangle" was active, it should switch to "arrow".
    controller.toggleTool(QStringLiteral("arrow"));
    QCOMPARE(controller.activeTool(), QStringLiteral("arrow"));
    QCOMPARE(controller.currentMode(), QStringLiteral("draw"));

    // 2. Test cycleColor()
    QCOMPARE(controller.defaultColor(), QStringLiteral("#e63946"));
    controller.cycleColor();
    QCOMPARE(controller.defaultColor(), QStringLiteral("#f4a261"));
    controller.cycleColor();
    QCOMPARE(controller.defaultColor(), QStringLiteral("#e9c46a"));

    // 3. Test growSelected() and shrinkSelected() with rectangle shape
    QVariantMap rect;
    rect[QStringLiteral("type")] = QStringLiteral("rectangle");
    rect[QStringLiteral("strokeWidth")] = 2;
    controller.addShape(rect); // This also selects it!
    QCOMPARE(controller.selectedIndex(), 0);

    QVariantMap state = controller.getSelectionState();
    QCOMPARE(state[QStringLiteral("strokeWidth")].toInt(), 2);

    controller.growSelected();
    state = controller.getSelectionState();
    QCOMPARE(state[QStringLiteral("strokeWidth")].toInt(), 3);

    controller.shrinkSelected();
    state = controller.getSelectionState();
    QCOMPARE(state[QStringLiteral("strokeWidth")].toInt(), 2);

    // 4. Test growSelected() and shrinkSelected() with text shape
    QVariantMap text;
    text[QStringLiteral("type")] = QStringLiteral("text");
    text[QStringLiteral("fontSize")] = 20;
    controller.addShape(text); // This selects it, moving selectedIndex to 1
    QCOMPARE(controller.selectedIndex(), 1);

    state = controller.getSelectionState();
    QCOMPARE(text.value(QStringLiteral("fontSize")).toInt(), 20); // wait, let's verify selected state properties
    QCOMPARE(state[QStringLiteral("fontSize")].toInt(), 20);

    controller.growSelected();
    state = controller.getSelectionState();
    QCOMPARE(state[QStringLiteral("fontSize")].toInt(), 22);

    controller.shrinkSelected();
    state = controller.getSelectionState();
    QCOMPARE(state[QStringLiteral("fontSize")].toInt(), 20);

    // 5. Test getShortcuts() has local and global shortcuts with correct types
    QVariantList shortcuts = controller.getShortcuts();
    QVERIFY(shortcuts.size() > 1); // should have 1 global + 18 local shortcuts
    
    // Check that we have a global shortcut
    bool foundGlobal = false;
    bool foundLocal = false;
    for (const auto &val : shortcuts) {
        QVariantMap map = val.toMap();
        if (map[QStringLiteral("type")].toString() == QStringLiteral("global")) {
            foundGlobal = true;
        } else if (map[QStringLiteral("type")].toString() == QStringLiteral("local")) {
            foundLocal = true;
        }
    }
    QVERIFY(foundGlobal);
    QVERIFY(foundLocal);

    // 6. Test selectPresetColor
    controller.selectPresetColor(0); // Red: #e63946
    QCOMPARE(controller.defaultColor(), QStringLiteral("#e63946"));
    controller.selectPresetColor(1); // Orange: #f4a261
    QCOMPARE(controller.defaultColor(), QStringLiteral("#f4a261"));

    // 7. Test localShortcutSequences property maps
    QVariantMap localSeqs = controller.localShortcutSequences();
    QCOMPARE(localSeqs[QStringLiteral("tool_arrow")].toString(), QStringLiteral("A"));
    QCOMPARE(localSeqs[QStringLiteral("tool_rectangle")].toString(), QStringLiteral("R"));

    // 8. Test changeShortcut conflict resolution and persistence
    // Check changing local shortcut
    controller.changeShortcut(QStringLiteral("tool_rectangle"), QStringLiteral("X"));
    QCOMPARE(controller.localShortcutSequences()[QStringLiteral("tool_rectangle")].toString(), QStringLiteral("X"));

    // Check conflict resolution within local shortcuts (assigning 'X' to tool_arrow should clear tool_rectangle)
    controller.changeShortcut(QStringLiteral("tool_arrow"), QStringLiteral("X"));
    QCOMPARE(controller.localShortcutSequences()[QStringLiteral("tool_arrow")].toString(), QStringLiteral("X"));
    QCOMPARE(controller.localShortcutSequences()[QStringLiteral("tool_rectangle")].toString(), QStringLiteral(""));

    // 9. Test cross-domain conflict resolution
    // Global -> Local:
    // First, set global shortcut to Ctrl+Shift+Y.
    controller.changeShortcut(QStringLiteral("action_dummy"), QStringLiteral("Ctrl+Shift+Y"));
    QTest::qWait(50);
    QList<QKeySequence> globShortcut = KGlobalAccel::self()->shortcut(&dummyAction);
    QCOMPARE(globShortcut.first().toString(QKeySequence::PortableText), QStringLiteral("Ctrl+Shift+Y"));

    // Now, assign the same Ctrl+Shift+Y to local tool_line
    controller.changeShortcut(QStringLiteral("tool_line"), QStringLiteral("Ctrl+Shift+Y"));
    QTest::qWait(50);
    // The local should be Ctrl+Shift+Y
    QCOMPARE(controller.localShortcutSequences()[QStringLiteral("tool_line")].toString(), QStringLiteral("Ctrl+Shift+Y"));
    // The global action should be cleared (no shortcuts)
    QVERIFY(KGlobalAccel::self()->shortcut(&dummyAction).isEmpty());

    // Local -> Global:
    // First, set local shortcut to Ctrl+Shift+H
    controller.changeShortcut(QStringLiteral("tool_ellipse"), QStringLiteral("Ctrl+Shift+H"));
    QCOMPARE(controller.localShortcutSequences()[QStringLiteral("tool_ellipse")].toString(), QStringLiteral("Ctrl+Shift+H"));

    // Now, assign Ctrl+Shift+H to global action_dummy
    controller.changeShortcut(QStringLiteral("action_dummy"), QStringLiteral("Ctrl+Shift+H"));
    QTest::qWait(50);
    // The global action should be Ctrl+Shift+H
    QCOMPARE(KGlobalAccel::self()->shortcut(&dummyAction).first().toString(QKeySequence::PortableText), QStringLiteral("Ctrl+Shift+H"));
    // The local tool_ellipse should be cleared
    QCOMPARE(controller.localShortcutSequences()[QStringLiteral("tool_ellipse")].toString(), QStringLiteral(""));

    // Reset to defaults using changeShortcut (or clean up QSettings for clean subsequent runs)
    QSettings cleanSettings(QStringLiteral("scribbleway"), QStringLiteral("shortcuts"));
    cleanSettings.clear();
    
    // Clear global KGlobalAccel state for clean subsequent runs
    KGlobalAccel::self()->setShortcut(&dummyAction, QList<QKeySequence>(), KGlobalAccel::NoAutoloading);
    QTest::qWait(50);
}
QTEST_MAIN(ShapesModelTest)
#include "shapesmodeltest.moc"
