#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QAction>
#include <QKeySequence>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QElapsedTimer>
#include <QMargins>
#include <QRect>
#include <QDebug>

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>
#include <KGlobalAccel>

#include <signal.h>

#include "overlaycontroller.h"
#include "singleinstance.h"

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);

    // Must call useLayerShell before QGuiApplication is created
    LayerShellQt::Shell::useLayerShell();

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("scribbleway"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));
    app.setDesktopFileName(QStringLiteral("scribbleway"));

    const bool toggleRequest = app.arguments().contains(QStringLiteral("--toggle"));

    SingleInstanceGuard instanceGuard(QStringLiteral("scribbleway"));
    if (!instanceGuard.tryAcquire(toggleRequest ? QByteArray("toggle") : QByteArray())) {
        if (!toggleRequest) {
            qWarning() << "Scribbleway is already running.";
        }
        return 0;
    }

    OverlayController controller;

    // Setup global actions and connect to KGlobalAccel
    auto setupGlobalAction = [&](const QString &objName, const QString &text, 
                                 const QList<QKeySequence> &defaultShortcuts) {
        QAction *action = new QAction(&app);
        action->setObjectName(objName);
        action->setText(text);
        
        KGlobalAccel::self()->setDefaultShortcut(action, defaultShortcuts);
        KGlobalAccel::self()->setShortcut(action, defaultShortcuts);
        
        controller.registerAction(action, objName, text);
        return action;
    };

    QAction *actionActivateSelect = setupGlobalAction(QStringLiteral("action_activate_select_mode"), QStringLiteral("Enter Selection Mode"), 
                                                      {QKeySequence(QStringLiteral("Meta+Shift+X"))});

    auto toggleMode = [&controller]() {
        if (controller.currentMode() == QStringLiteral("passthrough")) {
            controller.enterSelectMode();
        } else {
            controller.enterPassthroughMode();
        }
    };
    QObject::connect(actionActivateSelect, &QAction::triggered, &controller, toggleMode);

    // KGlobalAccel is inert outside Plasma, so `scribbleway-overlay --toggle` re-uses the
    // single-instance socket as the entry point other compositors can bind:
    //   bind = SUPER SHIFT, X, exec, scribbleway-overlay --toggle
    QObject::connect(instanceGuard.server(), &QLocalServer::newConnection, &app, [&instanceGuard, toggleMode]() {
        QLocalSocket *client = instanceGuard.server()->nextPendingConnection();
        if (!client) return;
        QObject::connect(client, &QLocalSocket::readyRead, client, [client, toggleMode]() {
            if (client->readAll().startsWith("toggle")) {
                toggleMode();
            }
        });
        QObject::connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    });
    // Load QML Engine
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    
    const QUrl url(QStringLiteral("qrc:/scribbleway/qml/main.qml"));
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, &controller](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
            return;
        }

        if (objUrl != url) {
            return; // Only the overlay window gets the fullscreen layer-shell treatment.
        }

        auto *window = qobject_cast<QQuickWindow*>(obj);
        if (window) {
            window->setScreen(QGuiApplication::primaryScreen());
            controller.setWindow(window);
            
            // Apply LayerShell configuration
            if (auto *layerWindow = LayerShellQt::Window::get(window)) {
                LayerShellQt::Window::Anchors anchors;
                anchors.setFlag(LayerShellQt::Window::AnchorTop);
                anchors.setFlag(LayerShellQt::Window::AnchorBottom);
                anchors.setFlag(LayerShellQt::Window::AnchorLeft);
                anchors.setFlag(LayerShellQt::Window::AnchorRight);
                layerWindow->setAnchors(anchors);
                layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
                // -1, not 0: an annotation overlay must cover the whole output. 0 means
                // "reserve nothing but keep me clear of other exclusive zones", which
                // makes any bar (Waybar, a Plasma panel) shrink the drawable area.
                layerWindow->setExclusiveZone(-1);
                layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
                layerWindow->setScope(QStringLiteral("scribbleway-overlay"));
            }
            
            // Set initial input region to be fully click-through (1x1 pixel offscreen)
            window->setMask(QRegion(-100, -100, 1, 1));

            // Show window after configuration
            window->show();
        }
    }, Qt::QueuedConnection);

    engine.load(url);

    // The Tray Popup is a small anchored layer-shell surface (menu-sized, corner-placed),
    // not the fullscreen overlay above. Hidden until the tray icon is clicked.
    const QUrl trayPopupUrl(QStringLiteral("qrc:/scribbleway/qml/TrayPopup.qml"));
    QQuickWindow *popupWindow = nullptr;
    QElapsedTimer hiddenTimer;
    auto hidePopup = [&hiddenTimer, &controller](QQuickWindow *w) {
        if (w && w->isVisible()) {
            w->hide();
            hiddenTimer.restart();
            controller.setPopupExclusion(QRect());
        }
    };
    // Anchor to the bottom-right corner: adjacent edges keep the window's natural size
    // (menu-sized), unlike the overlay's opposite-edge fullscreen anchoring. Re-applied
    // before every show so it survives the destroy()/recreate used to move screens.
    auto configurePopupLayer = [&popupWindow]() {
        if (!popupWindow) return;
        if (auto *layerWindow = LayerShellQt::Window::get(popupWindow)) {
            LayerShellQt::Window::Anchors anchors;
            anchors.setFlag(LayerShellQt::Window::AnchorBottom);
            anchors.setFlag(LayerShellQt::Window::AnchorRight);
            layerWindow->setAnchors(anchors);
            layerWindow->setMargins(QMargins(0, 0, 8, 8));
            layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
            layerWindow->setExclusiveZone(0);
            layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
            // Qt-Wayland ignores setScreen() (compositor owns placement), so let KWin
            // pick the output — it maps a null-output layer surface on the active screen.
            layerWindow->setScreenConfiguration(LayerShellQt::Window::ScreenFromCompositor);
            layerWindow->setScope(QStringLiteral("scribbleway-traypopup"));
        }
    };
    // Qt-Wayland ignores QWindow::setScreen() (the compositor owns window placement),
    // and Wayland gives no global cursor position or tray-icon geometry — so we can't
    // force or detect the screen. Instead configurePopupLayer() uses ScreenFromCompositor,
    // which makes KWin map the popup on whichever output is currently active. That is the
    // behaviour we want: the popup follows the screen you're working on.
    auto showPopup = [&popupWindow, &controller, &configurePopupLayer]() {
        if (!popupWindow) return;

        configurePopupLayer();
        popupWindow->show();
        popupWindow->raise();
        popupWindow->requestActivate();

        // Keep the popup clickable even when a drawing tool has the overlay grabbing
        // input: carve the popup's rect out of the overlay's input region when they
        // share a screen (bottom-right corner, matching the popup's layer-shell anchor).
        QRect exclusion;
        if (QQuickWindow *ov = controller.window()) {
            if (ov->screen() == popupWindow->screen()) {
                const int pw = popupWindow->width();
                const int ph = popupWindow->height();
                exclusion = QRect(ov->width() - pw - 8, ov->height() - ph - 8, pw, ph);
            }
        }
        controller.setPopupExclusion(exclusion);
    };
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [trayPopupUrl, &popupWindow, &hidePopup, &configurePopupLayer](QObject *obj, const QUrl &objUrl) {
        if (objUrl != trayPopupUrl) return;
        popupWindow = qobject_cast<QQuickWindow*>(obj);
        if (!popupWindow) return;

        configurePopupLayer();

        // Dismiss on focus loss (click outside).
        QObject::connect(popupWindow, &QWindow::activeChanged, popupWindow, [&popupWindow, &hidePopup]() {
            if (popupWindow && !popupWindow->isActive()) {
                hidePopup(popupWindow);
            }
        });
    }, Qt::QueuedConnection);
    engine.load(trayPopupUrl);

    QSystemTrayIcon trayIcon(QIcon::fromTheme(QStringLiteral("draw-freehand"),
                                              QIcon(QStringLiteral(":/icons/draw-freehand.svg"))));
    trayIcon.setToolTip(QStringLiteral("Scribbleway"));
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, &app,
                     [&popupWindow, &hidePopup, &hiddenTimer, &showPopup](QSystemTrayIcon::ActivationReason reason) {
        if (reason != QSystemTrayIcon::Trigger) return;
        if (!popupWindow) return;

        // If focus-out just hid it as part of this same click, don't reopen.
        const bool recentlyHidden = hiddenTimer.isValid() && hiddenTimer.elapsed() < 250;
        if (popupWindow->isVisible() || recentlyHidden) {
            hidePopup(popupWindow);
            return;
        }
        showPopup();
    });
    // Shown regardless: isSystemTrayAvailable() can be a false negative when the
    // StatusNotifierHost registers a moment later (documented Waybar/Hyprland race).
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "No system tray detected. Scribbleway's controls (color, tools, "
                      "shortcuts, shape list) will not be reachable without one. "
                      "On Hyprland, enable a StatusNotifierHost — e.g. Waybar's "
                      "\"tray\" module — then restart Scribbleway.";
    }
    trayIcon.show();

    return app.exec();
}
