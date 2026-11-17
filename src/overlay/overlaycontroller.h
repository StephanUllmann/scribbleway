#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QQuickWindow>
#include <QRect>
#include <QHash>
#include <QSet>
#include <limits>
#include "shapesmodel.h"

struct BindingHit {
    QString targetId;
    double focus = 0.0;
    QPointF snapPoint;
    bool valid = false;
};

class OverlayController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(ShapesModel* shapesModel READ shapesModel CONSTANT)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectionChanged)
    Q_PROPERTY(QString activeTool READ activeTool WRITE setActiveTool NOTIFY activeToolChanged)
    Q_PROPERTY(QString currentMode READ currentMode NOTIFY modeChanged)
    // The style new Shapes get, and the Tray Popup shows with nothing selected. Read-only:
    // QML writes it through updateProperties(), the one path that also updates the selection.
    Q_PROPERTY(QVariantMap style READ style NOTIFY styleChanged)
    Q_PROPERTY(bool hasMultiSelection READ hasMultiSelection NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap localShortcutSequences READ localShortcutSequences CONSTANT)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selection READ getSelectionState NOTIFY selectionChanged)
    Q_PROPERTY(QStringList presetColors READ presetColors CONSTANT)

public:
    explicit OverlayController(QObject *parent = nullptr);

    QQuickWindow* window() const;
    void setWindow(QQuickWindow *window);

    int selectedIndex() const;
    bool hasMultiSelection() const;

    bool hasSelection() const;
    // The selected shape's properties, or the defaults when nothing is selected.
    QVariantMap getSelectionState() const;
    // The same list selectPresetColor()/the number hotkeys index, so the swatches
    // in the menu cannot drift out of step with them.
    static QStringList presetColors();

    QString currentMode() const;

    QVariantMap style() const;
    // The style keys a Shape of `type` carries, from style().
    Q_INVOKABLE QVariantMap styleForNewShape(const QString &type) const;

    ShapesModel* shapesModel();
    QVariantMap localShortcutSequences() const;

    Q_INVOKABLE int indexForId(const QString &id) const;
    Q_INVOKABLE QVariantMap findSnapInfo(double px, double py, int excludeIndex = -1) const;
    Q_INVOKABLE void createBindingsForShape(int lineIndex);
    Q_INVOKABLE void breakEndpointBinding(int lineIndex, bool isStart);
    Q_INVOKABLE void resnapEndpoint(int lineIndex, bool isStart);

    // QML-invokable methods
    Q_INVOKABLE void setSelectedIndex(int index);
    Q_INVOKABLE void beginEdit();
    Q_INVOKABLE void endEdit();
    Q_INVOKABLE QVariantMap getShape(int index) const;
    Q_INVOKABLE QVariantMap attachedTextForShape(int index) const;
    Q_INVOKABLE QRectF attachedTextRect(int index) const;
    Q_INVOKABLE QVariantMap ensureAttachedTextForShape(int index);
    Q_INVOKABLE void setAttachedText(int index, const QString &text);
    Q_INVOKABLE void removeAttachedText(int index);
    Q_INVOKABLE void copySelected();
    Q_INVOKABLE void pasteFromClipboard(double localX = -1.0, double localY = -1.0);
    Q_INVOKABLE void selectShapesInRect(double rx, double ry, double rw, double rh, bool shiftHeld);
    Q_INVOKABLE void beginDragSelection(bool shiftHeld);
    Q_INVOKABLE void dragSelected(double dx, double dy);
    Q_INVOKABLE void nudgeSelected(double dx, double dy);
    Q_INVOKABLE void toggleTool(const QString &tool);
    Q_INVOKABLE void cycleColor();
    Q_INVOKABLE void growSelected();
    Q_INVOKABLE void shrinkSelected();
    Q_INVOKABLE void increaseBorderRadius();
    Q_INVOKABLE void decreaseBorderRadius();
    Q_INVOKABLE void cycleRoughness();
    Q_INVOKABLE void selectPresetColor(int index);

public Q_SLOTS:
    QString activeTool() const;
    void setActiveTool(const QString &tool);

    void addShape(const QVariantMap &shape);
    void updateShape(int index, const QVariantMap &properties);
    void updateProperties(const QVariantMap &properties);
    void undo();
    void redo();
    void clear();
    void deleteSelected();
    void raiseSelected();
    void lowerSelected();

    void enterSelectMode();
    void enterPassthroughMode();

    void setShapeLocked(int index, bool locked);
    void deleteShape(int index);
    void selectShape(int index, bool shiftHeld = false);

Q_SIGNALS:
    void windowChanged();
    void activeToolChanged(const QString &tool);
    void styleChanged();
    
    void selectionChanged();
    void modeChanged(const QString &mode);

    // Command signals to QML
    void enterSelectModeRequested();
    void enterPassthroughModeRequested();

private:
    void notifySelectionChanged();
    void ensureSelectMode();
    bool selectedTypeIs(const QString &type) const;
    // Clamps and converts a style value and stores it; true when the style changed.
    bool writeStyle(const QString &key, const QVariant &value);
    // grow / shrink / border-radius are all the same move: read one int off the
    // selection, add a delta, clamp, write it back.
    void bumpSelected(const QString &key, int delta, int lo,
                      int hi = std::numeric_limits<int>::max());
    // An arrow moved away from a shape that stayed put lets go of that shape.
    void detachFromUnmoved(int index, QVariantMap &shape, const QSet<int> &moved);
    void moveShapes(const QHash<int, QVariantMap> &starts, double dx, double dy);
    QVariantMap makeAttachedTextBinding(const QVariantMap &shape) const;

    QVariantMap m_localShortcutSequences;

    QQuickWindow *m_window = nullptr;
    ShapesModel m_shapesModel;
    int m_selectedIndex = -1;
    QHash<int, QVariantMap> m_dragStartShapes;
    QList<QVariantMap> m_editStartShapes; // the whole model at beginEdit(), for the dev log
    QSet<int> m_preDragSelection;

    QString m_activeTool;
    QString m_currentMode = QStringLiteral("passthrough");
    QVariantMap m_style;

    static constexpr double kSnapThreshold = 20.0;
    BindingHit findSnapTarget(double px, double py, int excludeIndex = -1) const;
    void updateBoundEndpoints(int shapeIndex, const QSet<int> *skipIndices = nullptr);
    void cleanupBindingsForDelete(int deletedIndex);
    void addBackReference(const QString &targetId, const QString &lineId);
    void removeBackReference(const QString &targetId, const QString &lineId);
};
