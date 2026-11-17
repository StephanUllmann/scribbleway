#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QQuickWindow>

#include "layersurface.h"

class OverlayWindow;

// The Tray Popup: the small menu window the tray icon opens. It owns the window's layer
// config, whether it is open, the rules that close it and the hole it keeps in the
// overlay's input region. Nothing else shows or hides the window.
class TrayPopup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(bool wantsKeyboard READ wantsKeyboard WRITE setWantsKeyboard NOTIFY wantsKeyboardChanged)

public:
    // The surface and the overlay are not owned and must outlive this object.
    TrayPopup(LayerSurface *surface, OverlayWindow *overlay, QObject *parent = nullptr);

    // Takes over the (hidden) popup window.
    void attach(QQuickWindow *window);

    // A click on the tray icon: opens the popup, or closes it if it is open.
    void toggle();
    // Escape, or a click on the overlay canvas.
    Q_INVOKABLE void close();

    bool isOpen() const;
    // True while a text field in the popup has focus; the overlay lets go of the keyboard
    // for that long.
    bool wantsKeyboard() const;
    void setWantsKeyboard(bool wants);

Q_SIGNALS:
    void openChanged();
    void wantsKeyboardChanged();

private:
    void hide(const char *reason);
    void onVisibleChanged(bool visible);
    void updateHole();

    LayerSurface *m_surface;
    OverlayWindow *m_overlay;
    QQuickWindow *m_window = nullptr;
    // Anchor to the bottom-right corner: adjacent edges keep the window's natural size
    // (menu-sized), unlike the overlay's opposite-edge fullscreen anchoring. The overlay
    // places the popup's input hole from this same config.
    //
    // Qt-Wayland ignores QWindow::setScreen() and Wayland gives no global cursor position
    // or tray-icon geometry, so the screen can't be forced or detected. onActiveScreen
    // makes the compositor map the popup on the output in use, which is what we want.
    const LayerConfig m_config{.anchors = Qt::BottomEdge | Qt::RightEdge,
                               .margins = QMargins(0, 0, 8, 8),
                               .exclusiveZone = 0,
                               .keyboard = LayerConfig::Keyboard::OnDemand,
                               .scope = QStringLiteral("scribbleway-traypopup"),
                               .onActiveScreen = true};
    bool m_wantsKeyboard = false;
    QElapsedTimer m_hiddenTimer;
};
