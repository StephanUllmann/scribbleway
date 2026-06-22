#pragma once

#include <QObject>
#include <QVariantList>
#include <QQuickWindow>
#include <QRegion>
#include <QRect>
#include <QAction>
#include "shapesmodel.h"

struct ShortcutAction {
    QAction *action = nullptr;
    QString id;
    QString displayName;
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

public:
    explicit OverlayController(QObject *parent = nullptr);

    QQuickWindow* window() const;
    void setWindow(QQuickWindow *window);

    QVariantList shapesMetadata() const;
    int selectedIndex() const;

    QString activeTool() const;
    void setActiveTool(const QString &tool);

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

    ShapesModel* shapesModel();

    void registerAction(QAction *action, const QString &actionId, const QString &displayName);

    // QML-invokable methods
    Q_INVOKABLE void setSelectedIndex(int index);
    Q_INVOKABLE void updateInputMask(const QVariantList &rects);
    Q_INVOKABLE void setKeyboardInteractivity(bool interactive);
    Q_INVOKABLE void beginEdit();
    Q_INVOKABLE void endEdit();
    Q_INVOKABLE QVariantMap getShape(int index) const;
    Q_INVOKABLE void copySelected();
    Q_INVOKABLE void pasteFromClipboard(double localX = -1.0, double localY = -1.0);

    // DBus-invokable slots (also used in C++)
public Q_SLOTS:
    QVariantList getShortcuts();
    void changeShortcut(const QString &actionId, const QString &shortcutString);

    void addShape(const QVariantMap &shape);
    void updateShape(int index, const QVariantMap &properties);
    QVariantList getShapesMetadata();
    QVariantMap getSelectionState();
    void setTool(const QString &tool);
    QString getActiveTool();
    void updateProperties(const QVariantMap &properties);
    void undo();
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
    void selectShape(int index);

Q_SIGNALS:
    void windowChanged();
    void activeToolChanged(const QString &tool);
    void defaultColorChanged();
    void defaultStrokeWidthChanged();
    void defaultOpacityChanged();
    void defaultFontFamilyChanged();
    void defaultFontSizeChanged();
    
    // DBus signals (matched by AppletBackend slots)
    void selectionChanged(const QVariantMap &selectionState);
    void shapesListChanged(const QVariantList &shapes);
    void shapesMetadataChanged(const QVariantList &metadata);
    void modeChanged(const QString &mode);
    void shortcutsChanged(const QVariantList &shortcuts);

    // Command signals to QML
    void startDrawingGesture(const QString &tool);
    void enterSelectModeRequested();
    void enterPassthroughModeRequested();

private:
    void notifySelectionChanged();
    void notifyShapesChanged();

    QList<ShortcutAction> m_shortcutActions;

    QQuickWindow *m_window = nullptr;
    ShapesModel m_shapesModel;
    int m_selectedIndex = -1;

    QString m_activeTool;
    QString m_currentMode = QStringLiteral("passthrough");
    QString m_defaultColor;
    int m_defaultStrokeWidth = 2;
    double m_defaultOpacity = 1.0;
    QString m_defaultFontFamily;
    int m_defaultFontSize = 20;

    QRegion m_lastInputMask;
};
