#include "traypopup.h"

#include <QGuiApplication>

#include "devlog.h"
#include "overlaywindow.h"

TrayPopup::TrayPopup(LayerSurface *surface, OverlayWindow *overlay, QObject *parent)
    : QObject(parent)
    , m_surface(surface)
    , m_overlay(overlay)
{
}

void TrayPopup::attach(QQuickWindow *window)
{
    m_window = window;
    m_surface->apply(m_window, m_config);

    // Width too, not just show: opening the colour picker widens the popup, and a hole
    // left at the old width leaves the new strip unclickable while a tool is active.
    connect(m_window, &QWindow::screenChanged, this, &TrayPopup::updateHole);
    connect(m_window, &QWindow::widthChanged, this, &TrayPopup::updateHole);
    connect(m_window, &QWindow::heightChanged, this, &TrayPopup::updateHole);
    connect(m_window, &QWindow::visibleChanged, this, &TrayPopup::onVisibleChanged);

    // Dismiss on focus loss (click outside) — but the overlay is our own window and
    // holds an exclusive keyboard grab whenever a tool is active, so it takes focus
    // constantly. Treating that as "clicked outside" is what made the popup close on
    // its own actions. A click on the overlay canvas closes it through close() instead
    // (see main.qml), which is what the user means by outside.
    connect(m_window, &QWindow::activeChanged, this, [this]() {
        if (m_window->isActive()) return;
        if (QGuiApplication::focusWindow() == m_overlay->window()) return;
        hide("focus moved elsewhere");
    });
}

void TrayPopup::toggle()
{
    if (!m_window) return;

    // If focus-out just hid it as part of this same click, don't reopen.
    const bool recentlyHidden = m_hiddenTimer.isValid() && m_hiddenTimer.elapsed() < 250;
    if (m_window->isVisible() || recentlyHidden) {
        hide("tray icon");
        return;
    }
    // Re-applied before every show so it survives the destroy()/recreate used to move screens.
    m_surface->apply(m_window, m_config);
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void TrayPopup::close()
{
    hide("escape or canvas click");
}

// Only hides; the bookkeeping runs in onVisibleChanged(), which also catches the popup
// vanishing without passing through here. The reason is logged so a "closed" line with
// no "close requested" before it shows such an outside close.
void TrayPopup::hide(const char *reason)
{
    if (m_window && m_window->isVisible()) {
        qCInfo(lcDev) << "popup close requested:" << reason;
        m_window->hide();
    }
}

// Every way the popup appears or disappears ends here, including the compositor closing
// the surface or Qt closing it as a Qt.Popup window, so the popup is never believed open
// (keeping its hole and withholding the keyboard) after it is gone.
void TrayPopup::onVisibleChanged(bool visible)
{
    updateHole();
    qCInfo(lcDev) << "tray popup" << (visible ? "open" : "closed");
    if (!visible) {
        m_hiddenTimer.restart();
        // A stuck claim here would leave the overlay with no hotkeys and no visible cause.
        setWantsKeyboard(false);
    }
    Q_EMIT openChanged();
}

void TrayPopup::updateHole()
{
    // Keeps the popup clickable even when a drawing tool has the overlay grabbing input.
    m_overlay->setHole(isOpen() ? m_window->screen() : nullptr, m_config, m_window->size());
}

bool TrayPopup::isOpen() const
{
    return m_window && m_window->isVisible();
}

bool TrayPopup::wantsKeyboard() const
{
    return m_wantsKeyboard;
}

// Only the popup's text fields take the keyboard away from the overlay, and only while
// they are focused. Yielding it for the whole time the popup is open killed every tool
// hotkey the moment the popup was touched, since the shortcuts live on the overlay window.
void TrayPopup::setWantsKeyboard(bool wants)
{
    if (m_wantsKeyboard == wants) return;
    m_wantsKeyboard = wants;
    qCInfo(lcDev) << "popup wants keyboard:" << wants;
    m_overlay->setKeyboardYielded(wants);
    Q_EMIT wantsKeyboardChanged();
}
