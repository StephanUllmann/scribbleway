#pragma once

#include <QObject>
#include <QStringList>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QtQml/qqmlregistration.h>

class AppletBackend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool overlayConnected READ overlayConnected NOTIFY overlayConnectedChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedType READ selectedType NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedColor READ selectedColor NOTIFY selectionChanged)
    Q_PROPERTY(int selectedStrokeWidth READ selectedStrokeWidth NOTIFY selectionChanged)
    Q_PROPERTY(double selectedOpacity READ selectedOpacity NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedFontFamily READ selectedFontFamily NOTIFY selectionChanged)
    Q_PROPERTY(int selectedFontSize READ selectedFontSize NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedLocked READ selectedLocked NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList shapesList READ shapesList NOTIFY shapesListChanged)
    Q_PROPERTY(int selectedShapeIndex READ selectedShapeIndex NOTIFY selectionChanged)
    Q_PROPERTY(int selectedBorderRadius READ selectedBorderRadius NOTIFY selectionChanged)
    Q_PROPERTY(int selectedGlow READ selectedGlow NOTIFY selectionChanged)
    Q_PROPERTY(int selectedFreehandSmoothing READ selectedFreehandSmoothing NOTIFY selectionChanged)
    Q_PROPERTY(int selectedRoughness READ selectedRoughness NOTIFY selectionChanged)
    Q_PROPERTY(QString currentMode READ currentMode NOTIFY modeChanged)
    Q_PROPERTY(QString activeTool READ activeTool NOTIFY activeToolChanged)
    Q_PROPERTY(QStringList screenNames READ screenNames CONSTANT)
    Q_PROPERTY(QString targetScreen READ targetScreen WRITE setTargetScreen NOTIFY targetScreenChanged)
    Q_PROPERTY(QVariantList shortcuts READ shortcuts NOTIFY shortcutsChanged)

public:
    explicit AppletBackend(QObject *parent = nullptr);

    bool overlayConnected() const;
    bool hasSelection() const;
    QString selectedType() const;
    QString selectedColor() const;
    int selectedStrokeWidth() const;
    double selectedOpacity() const;
    QString selectedFontFamily() const;
    int selectedFontSize() const;
    bool selectedLocked() const;
    QVariantList shapesList() const;
    int selectedShapeIndex() const;
    int selectedBorderRadius() const;
    int selectedGlow() const;
    int selectedFreehandSmoothing() const;
    int selectedRoughness() const;
    QString currentMode() const;
    QString activeTool() const;

    QStringList screenNames() const;
    QString targetScreen() const;
    QVariantList shortcuts() const;
    Q_INVOKABLE void setTargetScreen(const QString &screenName);


    Q_INVOKABLE void setTool(const QString &tool);
    Q_INVOKABLE void setColor(const QString &color);
    Q_INVOKABLE void setStrokeWidth(int width);
    Q_INVOKABLE void setOpacity(double opacity);
    Q_INVOKABLE void setFontFamily(const QString &family);
    Q_INVOKABLE void setFontSize(int size);
    Q_INVOKABLE void setBorderRadius(int radius);
    Q_INVOKABLE void setGlow(int glow);
    Q_INVOKABLE void setFreehandSmoothing(int level);
    Q_INVOKABLE void setRoughness(int roughness);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void toggleLock();
    Q_INVOKABLE void raiseSelected();
    Q_INVOKABLE void lowerSelected();
    
    Q_INVOKABLE void setShapeLocked(int index, bool locked);
    Q_INVOKABLE void deleteShape(int index);
    Q_INVOKABLE void selectShape(int index);
    Q_INVOKABLE void enterSelectMode();
    Q_INVOKABLE void enterPassthroughMode();
    Q_INVOKABLE void changeShortcut(const QString &actionId, const QString &shortcutString);
    Q_INVOKABLE QString formatKeySequence(int key, int modifiers) const;
    Q_INVOKABLE void startDaemon();

Q_SIGNALS:
    void overlayConnectedChanged();
    void selectionChanged();
    void shapesListChanged();
    void modeChanged();
    void targetScreenChanged();
    void activeToolChanged();
    void shortcutsChanged();

public Q_SLOTS:
    void onServiceRegistered(const QString &serviceName);
    void onServiceUnregistered(const QString &serviceName);
    void onSelectionChanged(const QVariantMap &state);
    void onShapesMetadataChanged(const QVariantList &metadata);
    void onModeChanged(const QString &mode);
    void onActiveToolChanged(const QString &tool);
    void onShortcutsChanged(const QVariantList &shortcuts);

private:
    void connectToOverlay();
    void sendDBus(const QString &method, const QVariantList &args = {});

    bool m_overlayConnected = false;
    bool m_hasSelection = false;
    QString m_selectedType;
    QString m_selectedColor;
    int m_selectedStrokeWidth = 2;
    double m_selectedOpacity = 1.0;
    QString m_selectedFontFamily;
    int m_selectedFontSize = 20;
    int m_selectedBorderRadius = 8;
    int m_selectedGlow = 10;
    int m_selectedFreehandSmoothing = 2;
    int m_selectedRoughness = 1;
    bool m_selectedLocked = false;
    int m_selectedShapeIndex = -1;
    QString m_currentMode = QStringLiteral("passthrough");
    QVariantList m_shapesList;
    QVariantList m_shortcuts;

    QDBusInterface *m_dbusInterface = nullptr;
    QString m_targetScreen;
    QString m_activeTool;
};
