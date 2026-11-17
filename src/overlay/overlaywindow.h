#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QRegion>
#include <QScreen>
#include <QTimer>
#include <QVariantList>

#include "layersurface.h"

// The overlay's fullscreen window as the compositor sees it: which parts of it take
// clicks, whether it holds the keyboard, its layer-shell config and the screen it is on.
// Nothing else writes these to the window.
class OverlayWindow : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList screenNames READ screenNames NOTIFY screensChanged)
    Q_PROPERTY(QStringList screenLabels READ screenLabels NOTIFY screensChanged)
    Q_PROPERTY(QString targetScreen READ targetScreen NOTIFY targetScreenChanged)

public:
    // The surface is not owned and must outlive this object.
    explicit OverlayWindow(LayerSurface *surface, QObject *parent = nullptr);

    // Takes over the window, places it on the primary screen and shows it. Whatever was
    // set before this is applied here.
    void attach(QQuickWindow *window);

    // The parts of the window that take clicks, in window coordinates. Empty means
    // click-through everywhere.
    Q_INVOKABLE void setShapeRegion(const QVariantList &rects);
    // A tool or the text editor needs the overlay's hotkeys.
    Q_INVOKABLE void setToolWantsKeyboard(bool wants);
    // Another surface (the Tray Popup) has the keyboard, so the overlay lets go of it
    // whatever the tools want.
    void setKeyboardYielded(bool yielded);
    // Keeps the area another layer surface covers clickable. The compositor places that
    // surface by its config inside the same area as the overlay, so the hole is placed the
    // same way inside the overlay's own size. A null screen, or one the overlay is not on,
    // means no hole.
    void setHole(QScreen *screen, const LayerConfig &placement, QSize size);

    QStringList screenNames() const;
    // Human-readable names in the same order as screenNames(), e.g. "Laptop screen (eDP-1)".
    QStringList screenLabels() const;
    QString targetScreen() const;
    QQuickWindow *window() const { return m_window; }

public Q_SLOTS:
    void setTargetScreen(const QString &screenName);
    // Gives the window a fresh surface on the target screen, or on the primary one while
    // the target is gone.
    void remap();

Q_SIGNALS:
    void screensChanged();
    void targetScreenChanged();

private:
    QRect holeRect() const;
    void applyInputMask();
    void applyKeyboard();
    void applyLayerConfig();

    LayerSurface *m_surface;
    QQuickWindow *m_window = nullptr;
    // 0, not -1: "reserve nothing, but keep me clear of other exclusive zones". With -1
    // the overlay covered every panel, so while a tool was active the tray icon that opens
    // the Tray Popup could not be clicked. The popup uses 0 too, so the compositor places
    // both in the same area and its hole lines up.
    LayerConfig m_config{.anchors = Qt::TopEdge | Qt::BottomEdge | Qt::LeftEdge | Qt::RightEdge,
                         .margins = {},
                         .exclusiveZone = 0,
                         .keyboard = LayerConfig::Keyboard::None,
                         .scope = QStringLiteral("scribbleway-overlay"),
                         .onActiveScreen = false};
    bool m_toolWantsKeyboard = false;
    bool m_keyboardYielded = false;

    QRegion m_shapeRegion;
    QRegion m_lastInputMask;
    QPointer<QScreen> m_holeScreen;
    LayerConfig m_holePlacement;
    QSize m_holeSize;
    QRect m_lastHole;

    QString m_targetScreen;
    QTimer m_remapTimer;
};
