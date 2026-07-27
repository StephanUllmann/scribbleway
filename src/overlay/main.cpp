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

    SingleInstanceGuard instanceGuard(QStringLiteral("scribbleway"));
    if (!instanceGuard.tryAcquire()) {
        qWarning() << "Scribbleway is already running.";
        return 0;
    }

    OverlayController controller;

    // Clean up legacy global shortcuts that are now local
    auto cleanLegacy = [&](const QString &objName) {
        QAction tmp;
        tmp.setObjectName(objName);
        tmp.setProperty("componentName", QStringLiteral("scribbleway"));
        KGlobalAccel::self()->removeAllShortcuts(&tmp);
    };
    cleanLegacy(QStringLiteral("draw_freehand"));
    cleanLegacy(QStringLiteral("draw_arrow"));
    cleanLegacy(QStringLiteral("draw_rectangle"));
    cleanLegacy(QStringLiteral("draw_ellipse"));
    cleanLegacy(QStringLiteral("draw_line"));
    cleanLegacy(QStringLiteral("draw_text"));
    cleanLegacy(QStringLiteral("action_undo"));
    cleanLegacy(QStringLiteral("action_clear"));
    cleanLegacy(QStringLiteral("action_select_mode"));
    cleanLegacy(QStringLiteral("action_cycle_color"));
    cleanLegacy(QStringLiteral("action_grow"));
    cleanLegacy(QStringLiteral("action_shrink"));

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

    QObject::connect(actionActivateSelect, &QAction::triggered, &controller, [&controller]() {
        if (controller.currentMode() == QStringLiteral("passthrough")) {
            controller.enterSelectMode();
        } else {
            controller.enterPassthroughMode();
        }
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
                layerWindow->setExclusiveZone(0);
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
    trayIcon.show();

    return app.exec();
}
