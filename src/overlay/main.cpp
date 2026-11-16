#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QAction>
#include <QKeySequence>
#include <QScreen>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QElapsedTimer>
#include <QMargins>
#include <QRect>
#include <QTimer>
#include <QDebug>
#include <QDir>

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>
#include <KGlobalAccel>

#include <signal.h>

#include "devlog.h"
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
    // Must match the installed org.kde.scribbleway.desktop, not the application name:
    // this is the Wayland app_id, and the portal looks the desktop file up by it.
    app.setDesktopFileName(QStringLiteral("org.kde.scribbleway"));

    const bool toggleRequest = app.arguments().contains(QStringLiteral("--toggle"));
    const bool markRequest = app.arguments().contains(QStringLiteral("--mark"));
    const bool devMode = app.arguments().contains(QStringLiteral("--dev"));

    // In the per-user runtime directory: a bare name lands in /tmp, shared by every user.
    SingleInstanceGuard instanceGuard(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
                                      + QStringLiteral("/scribbleway.sock"));
    const QByteArray handoff = markRequest ? QByteArray("mark") : toggleRequest ? QByteArray("toggle") : QByteArray();
    if (!instanceGuard.tryAcquire(handoff)) {
        if (!instanceGuard.foundRunningInstance()) {
            qCritical() << "Cannot open the single-instance socket:" << instanceGuard.server()->errorString();
            return 1;
        }
        if (devMode) {
            qWarning() << "Scribbleway is already running; quit it first to restart it with --dev.";
        } else if (handoff.isEmpty()) {
            qWarning() << "Scribbleway is already running.";
        }
        return 0;
    }
    if (markRequest) {
        qWarning() << "Scribbleway isn't running, so there is no log to mark.";
        return 0;
    }

    // Installed only after winning the instance check, so a second launch that just
    // hands off --toggle or --mark doesn't write into the running instance's log.
    if (devMode) {
        QString stateDir = qEnvironmentVariable("XDG_STATE_HOME");
        if (stateDir.isEmpty()) stateDir = QDir::homePath() + QStringLiteral("/.local/state");
        DevLog::install(stateDir + QStringLiteral("/scribbleway/logs"));
        qCInfo(lcDev).noquote() << "=== session start: scribbleway" << SCRIBBLEWAY_BUILD
                                << "pid" << QCoreApplication::applicationPid();
        qCInfo(lcDev).noquote() << "Qt" << qVersion() << "platform" << QGuiApplication::platformName()
                                << "desktop" << qEnvironmentVariable("XDG_CURRENT_DESKTOP")
                                << "session" << qEnvironmentVariable("XDG_SESSION_TYPE");
        DevLog::logScreens("startup");
    }
    // Numbered so a bug report can say "after mark 3".
    int markCount = 0;
    auto mark = [&markCount]() { qCInfo(lcDev).noquote() << "--- MARK" << ++markCount << "---"; };

    OverlayController controller;

    // Setup global actions and connect to KGlobalAccel
    auto setupGlobalAction = [&app](const QString &objName, const QString &text, 
                                 const QList<QKeySequence> &defaultShortcuts) {
        QAction *action = new QAction(&app);
        action->setObjectName(objName);
        action->setText(text);
        
        KGlobalAccel::self()->setDefaultShortcut(action, defaultShortcuts);
        KGlobalAccel::self()->setShortcut(action, defaultShortcuts);
        
        return action;
    };

    QAction *actionActivateSelect = setupGlobalAction(QStringLiteral("action_activate_select_mode"), QStringLiteral("Enter Selection Mode"), 
                                                      {QKeySequence(QStringLiteral("Meta+Shift+X"))});

    auto toggleMode = [&controller]() {
        qInfo() << "toggle requested, current mode:" << controller.currentMode();
        if (controller.currentMode() == QStringLiteral("passthrough")) {
            controller.enterSelectMode();
        } else {
            controller.enterPassthroughMode();
        }
    };
    QObject::connect(actionActivateSelect, &QAction::triggered, &controller, toggleMode);

    // Only registered in dev mode, so a normal install doesn't claim another global key.
    if (devMode) {
        QAction *actionMark = setupGlobalAction(QStringLiteral("action_dev_mark"), QStringLiteral("Mark the Dev Log"),
                                                {QKeySequence(QStringLiteral("Meta+Shift+M"))});
        QObject::connect(actionMark, &QAction::triggered, &app, mark);
    }

    // KGlobalAccel is inert outside Plasma, so `scribbleway-overlay --toggle` re-uses the
    // single-instance socket as the entry point other compositors can bind:
    //   bind = SUPER SHIFT, X, exec, scribbleway-overlay --toggle
    QObject::connect(instanceGuard.server(), &QLocalServer::newConnection, &app, [&instanceGuard, toggleMode, mark]() {
        QLocalSocket *client = instanceGuard.server()->nextPendingConnection();
        if (!client) return;
        QObject::connect(client, &QLocalSocket::readyRead, client, [client, toggleMode, mark]() {
            const QByteArray request = client->readAll();
            if (request.startsWith("toggle")) {
                toggleMode();
            } else if (request.startsWith("mark")) {
                mark();
            }
        });
        QObject::connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    });
    // Load QML Engine
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("devMode"), devMode);
    
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
            // Geometry, not setScreen(): see OverlayController::remapWindow().
            window->setGeometry(QGuiApplication::primaryScreen()->geometry());
            controller.setWindow(window);
            qCInfo(lcDev) << "overlay placed on" << window->screen()->name() << window->geometry();
            
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

    // A display switch fires a burst of these, briefly with no screens at all, so wait
    // for it to settle before giving the overlay a new surface.
    QTimer remapTimer;
    remapTimer.setSingleShot(true);
    remapTimer.setInterval(500);
    QObject::connect(&remapTimer, &QTimer::timeout, &controller, &OverlayController::remapWindow);
    QObject::connect(&app, &QGuiApplication::screenAdded, &app,
                     [](QScreen *s) { qCInfo(lcDev) << "screen added:" << s->name(); });
    QObject::connect(&app, &QGuiApplication::screenRemoved, &app,
                     [](QScreen *s) { qCInfo(lcDev) << "screen removed:" << s->name(); });
    QObject::connect(&app, &QGuiApplication::primaryScreenChanged, &app,
                     [](QScreen *s) { qCInfo(lcDev) << "primary screen now:" << (s ? s->name() : QString()); });
    QObject::connect(&app, &QGuiApplication::screenAdded, &remapTimer, qOverload<>(&QTimer::start));
    QObject::connect(&app, &QGuiApplication::screenRemoved, &remapTimer, qOverload<>(&QTimer::start));
    QObject::connect(&app, &QGuiApplication::primaryScreenChanged, &remapTimer, qOverload<>(&QTimer::start));

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
            controller.setTrayPopupOpen(false);
        }
    };
    // Escape inside the popup, and Escape or a click on the overlay canvas, all close it
    // from QML — which has no handle on this window.
    QObject::connect(&controller, &OverlayController::trayPopupCloseRequested, &app,
                     [&popupWindow, &hidePopup]() { hidePopup(popupWindow); });
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
            // Qt-Wayland ignores setScreen() (compositor owns placement), so let the
            // compositor pick the output — it maps a null-output layer surface on the
            // active screen. setScreenConfiguration() is the pre-6.6 spelling of this.
#ifdef HAVE_LAYERSHELL_ACTIVE_SCREEN
            layerWindow->setWantsToBeOnActiveScreen(true);
#else
            layerWindow->setScreenConfiguration(LayerShellQt::Window::ScreenFromCompositor);
#endif
            layerWindow->setScope(QStringLiteral("scribbleway-traypopup"));
        }
    };
    // Qt-Wayland ignores QWindow::setScreen() (the compositor owns window placement),
    // and Wayland gives no global cursor position or tray-icon geometry — so we can't
    // force or detect the screen. Instead configurePopupLayer() uses ScreenFromCompositor,
    // which makes KWin map the popup on whichever output is currently active. That is the
    // behaviour we want: the popup follows the screen you're working on.
    // Keep the popup clickable even when a drawing tool has the overlay grabbing input:
    // carve the popup's rect out of the overlay's input region when they share a screen
    // (bottom-right corner, matching the popup's layer-shell anchor). Re-run on every
    // width change, not just on show — opening the colour picker widens the popup, and a
    // rect left at the old width leaves the new strip unclickable, but only while a tool
    // is active, which is exactly where nobody looks for it.
    auto updatePopupExclusion = [&popupWindow, &controller]() {
        if (!popupWindow || !popupWindow->isVisible()) return;

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
    auto showPopup = [&popupWindow, &controller, &configurePopupLayer, &updatePopupExclusion]() {
        if (!popupWindow) return;

        configurePopupLayer();
        // Before show(): the overlay must drop its exclusive keyboard grab, or the
        // compositor never hands focus to the popup.
        controller.setTrayPopupOpen(true);
        popupWindow->show();
        popupWindow->raise();
        popupWindow->requestActivate();

        updatePopupExclusion();
    };
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [trayPopupUrl, &popupWindow, &hidePopup, &configurePopupLayer,
                            &updatePopupExclusion, &controller](QObject *obj, const QUrl &objUrl) {
        if (objUrl != trayPopupUrl) return;
        popupWindow = qobject_cast<QQuickWindow*>(obj);
        if (!popupWindow) return;

        configurePopupLayer();

        QObject::connect(popupWindow, &QWindow::widthChanged, popupWindow, updatePopupExclusion);

        // Dismiss on focus loss (click outside) — but the overlay is our own window and
        // holds an exclusive keyboard grab whenever a tool is active, so it takes focus
        // constantly. Treating that as "clicked outside" is what made the popup close on
        // its own actions. A click on the overlay canvas dismisses via the explicit
        // close request instead (see main.qml), which is what the user means by outside.
        QObject::connect(popupWindow, &QWindow::activeChanged, popupWindow,
                         [&popupWindow, &hidePopup, &controller]() {
            if (!popupWindow || popupWindow->isActive()) return;
            if (QGuiApplication::focusWindow() == controller.window()) return;
            hidePopup(popupWindow);
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

    qInfo() << "scribbleway-overlay ready";
    return app.exec();
}
