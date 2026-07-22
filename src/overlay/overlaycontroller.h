#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QQuickWindow>
#include <QRegion>
#include <QRect>
#include <QAction>
#include <QHash>
#include <QSet>
#include "shapesmodel.h"

struct ShortcutAction {
    QAction *action = nullptr;
    QString id;
    QString displayName;
};

struct LocalShortcutDef {
    QString id;
    QString displayName;
    QString defaultSequence;
    QString currentSequence;
};

struct BindingHit {
    QString targetId;
    double focus = 0.0;
    QPointF snapPoint;
    bool valid = false;
};

class OverlayController : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.scribbleway.OverlayController")
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(ShapesModel* shapesModel READ shapesModel CONSTANT)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectionChanged)
    Q_PROPERTY(QString activeTool READ activeTool WRITE setActiveTool NOTIFY activeToolChanged)
    Q_PROPERTY(QString currentMode READ currentMode NOTIFY modeChanged)
    Q_PROPERTY(QString defaultColor READ defaultColor WRITE setDefaultColor NOTIFY defaultColorChanged)
    Q_PROPERTY(int defaultStrokeWidth READ defaultStrokeWidth WRITE setDefaultStrokeWidth NOTIFY defaultStrokeWidthChanged)
    Q_PROPERTY(double defaultOpacity READ defaultOpacity WRITE setDefaultOpacity NOTIFY defaultOpacityChanged)
    Q_PROPERTY(QString defaultFontFamily READ defaultFontFamily WRITE setDefaultFontFamily NOTIFY defaultFontFamilyChanged)
    Q_PROPERTY(int defaultFontSize READ defaultFontSize WRITE setDefaultFontSize NOTIFY defaultFontSizeChanged)
    Q_PROPERTY(int defaultBorderRadius READ defaultBorderRadius WRITE setDefaultBorderRadius NOTIFY defaultBorderRadiusChanged)
    Q_PROPERTY(int defaultRoughness READ defaultRoughness WRITE setDefaultRoughness NOTIFY defaultRoughnessChanged)
    Q_PROPERTY(int defaultGlow READ defaultGlow WRITE setDefaultGlow NOTIFY defaultGlowChanged)
    Q_PROPERTY(QString defaultFillColor READ defaultFillColor WRITE setDefaultFillColor NOTIFY defaultFillColorChanged)
    Q_PROPERTY(double defaultFillOpacity READ defaultFillOpacity WRITE setDefaultFillOpacity NOTIFY defaultFillOpacityChanged)
    Q_PROPERTY(int defaultFreehandSmoothing READ defaultFreehandSmoothing WRITE setDefaultFreehandSmoothing NOTIFY defaultFreehandSmoothingChanged)
    Q_PROPERTY(bool hasMultiSelection READ hasMultiSelection NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedShapeType READ selectedShapeType NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap localShortcutSequences READ localShortcutSequences NOTIFY localShortcutsChanged)

public:
    explicit OverlayController(QObject *parent = nullptr);

    QQuickWindow* window() const;
    void setWindow(QQuickWindow *window);

    int selectedIndex() const;
    bool hasMultiSelection() const;
    QString selectedShapeType() const;

    QString currentMode() const;

    QString defaultColor() const;
    void setDefaultColor(const QString &color);

    int defaultStrokeWidth() const;
    void setDefaultStrokeWidth(int width);

    double defaultOpacity() const;
    void setDefaultOpacity(double opacity);

    QString defaultFontFamily() const;
    void setDefaultFontFamily(const QString &family);

    int defaultFontSize() const;
    void setDefaultFontSize(int size);
    
    int defaultBorderRadius() const;

    int defaultRoughness() const;
    void setDefaultRoughness(int roughness);
    int defaultGlow() const;
    void setDefaultGlow(int glow);
    QString defaultFillColor() const;
    void setDefaultFillColor(const QString &color);
    double defaultFillOpacity() const;
    void setDefaultFillOpacity(double opacity);
    int defaultFreehandSmoothing() const;
    void setDefaultFreehandSmoothing(int level);
    void setDefaultBorderRadius(int radius);

    ShapesModel* shapesModel();
    QVariantMap localShortcutSequences() const;

    void registerAction(QAction *action, const QString &actionId, const QString &displayName);
    Q_INVOKABLE int indexForId(const QString &id) const;
    Q_INVOKABLE QPointF pointFromBinding(const QVariantMap &targetShape, double focus) const;
    Q_INVOKABLE QPointF findSnapPoint(double px, double py, int excludeIndex = -1) const;
    Q_INVOKABLE QVariantMap findSnapInfo(double px, double py, int excludeIndex = -1) const;
    Q_INVOKABLE void createBindingsForShape(int lineIndex);
    Q_INVOKABLE void breakEndpointBinding(int lineIndex, bool isStart);
    Q_INVOKABLE void resnapEndpoint(int lineIndex, bool isStart);

    // QML-invokable methods
    Q_INVOKABLE void setSelectedIndex(int index);
    Q_INVOKABLE void updateInputMask(const QVariantList &rects);
    Q_INVOKABLE void setKeyboardInteractivity(bool interactive);
    Q_INVOKABLE void beginEdit();
    Q_INVOKABLE void endEdit();
    Q_INVOKABLE QVariantMap getShape(int index) const;
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

    // DBus-invokable slots (also used in C++)
public Q_SLOTS:
    QVariantList shapesMetadata() const;
    QString activeTool() const;
    void setActiveTool(const QString &tool);
    QVariantList getShortcuts();
    void changeShortcut(const QString &actionId, const QString &shortcutString);

    void addShape(const QVariantMap &shape);
    void updateShape(int index, const QVariantMap &properties);
    QVariantMap getSelectionState();
    void updateProperties(const QVariantMap &properties);
    void undo();
    void redo();
    void clear();
    void deleteSelected();
    void toggleLock();
    void raiseSelected();
    void lowerSelected();
    void setTargetScreen(const QString &screenName);

    void enterSelectMode();
    void enterPassthroughMode();

    void setShapeLocked(int index, bool locked);
    void deleteShape(int index);
    void selectShape(int index, bool shiftHeld = false);

Q_SIGNALS:
    void windowChanged();
    void activeToolChanged(const QString &tool);
    void defaultColorChanged();
    void defaultStrokeWidthChanged();
    void defaultOpacityChanged();
    void defaultFontFamilyChanged();
    void defaultFontSizeChanged();
    void defaultBorderRadiusChanged();
    void defaultRoughnessChanged();
    void defaultGlowChanged();
    void defaultFillColorChanged();
    void defaultFillOpacityChanged();
    void defaultFreehandSmoothingChanged();
    
    // DBus signals (matched by AppletBackend slots)
    void selectionChanged(const QVariantMap &selectionState);
    void shapesListChanged(const QVariantList &shapes);
    void shapesMetadataChanged(const QVariantList &metadata);
    void modeChanged(const QString &mode);
    void shortcutsChanged(const QVariantList &shortcuts);
    void localShortcutsChanged();

    // Command signals to QML
    void enterSelectModeRequested();
    void enterPassthroughModeRequested();

private:
    void notifySelectionChanged();
    void notifyShapesChanged();
    void ensureSelectMode();
    QJsonObject convertToExcalidraw(const QVariantMap &shape);
    QVariantMap convertFromExcalidraw(const QJsonObject &elem);

    QList<ShortcutAction> m_shortcutActions;
    QList<LocalShortcutDef> m_localShortcuts;

    QQuickWindow *m_window = nullptr;
    ShapesModel m_shapesModel;
    int m_selectedIndex = -1;
    QHash<int, QVariantMap> m_dragStartShapes;
    QSet<int> m_preDragSelection;

    QString m_activeTool;
    QString m_currentMode = QStringLiteral("passthrough");
    QString m_defaultColor;
    int m_defaultStrokeWidth = 2;
    double m_defaultOpacity = 1.0;
    QString m_defaultFontFamily;
    int m_defaultFontSize = 20;
    int m_defaultBorderRadius = 8;
    int m_defaultRoughness = 1;
    int m_defaultGlow = 10;
    QString m_defaultFillColor;
    double m_defaultFillOpacity = 0.12;
    int m_defaultFreehandSmoothing = 2;

    QRegion m_lastInputMask;

    static constexpr double kSnapThreshold = 20.0;
    BindingHit findSnapTarget(double px, double py, int excludeIndex = -1) const;
    double focusForPoint(const QVariantMap &targetShape, double px, double py) const;
    QPointF nearestPointOnRect(double rx, double ry, double rw, double rh, double px, double py) const;
    QPointF nearestPointOnEllipse(double cx, double cy, double a, double b, double px, double py) const;
    void updateBoundEndpoints(int shapeIndex, const QSet<int> *skipIndices = nullptr);
    void cleanupBindingsForDelete(int deletedIndex);
    void breakBinding(int lineIndex, bool isStart);
    void addBackReference(const QString &targetId, const QString &lineId);
    void removeBackReference(const QString &targetId, const QString &lineId);
};
