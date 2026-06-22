#include <QtTest>
#include <QSignalSpy>
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
    QCOMPARE(doc.object().value(QStringLiteral("type")).toString(), QStringLiteral("rectangle"));

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

QTEST_MAIN(ShapesModelTest)
#include "shapesmodeltest.moc"
