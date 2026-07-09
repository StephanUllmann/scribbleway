#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QAction>
#include <QKeySequence>
#include <QScreen>

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>
#include <KGlobalAccel>
#include <KDBusService>
#include <QDBusConnection>

#include <signal.h>

#include "overlaycontroller.h"

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);

    // Must call useLayerShell before QGuiApplication is created
    LayerShellQt::Shell::useLayerShell();

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("scribbleway"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));
    app.setDesktopFileName(QStringLiteral("scribbleway"));

    // Ensure unique application instance and register org.kde.scribbleway service
    KDBusService dbusService(KDBusService::Unique);

    OverlayController controller;

    // Register QObject on D-Bus Session Bus
    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/Overlay"),
        &controller,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
    );

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

    return app.exec();
}
