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

    // Setup global actions and connect to KGlobalAccel
    auto setupGlobalAction = [&](const QString &objName, const QString &text, 
                                 const QKeySequence &defaultShortcut, const QString &toolName = QString()) {
        QAction *action = new QAction(&app);
        action->setObjectName(objName);
        action->setText(text);
        
        QList<QKeySequence> shortcuts = {defaultShortcut};
        KGlobalAccel::self()->setDefaultShortcut(action, shortcuts);
        KGlobalAccel::self()->setShortcut(action, shortcuts);
        
        controller.registerAction(action, objName, text);
        
        if (!toolName.isEmpty()) {
            QObject::connect(action, &QAction::triggered, &controller, [&controller, toolName]() {
                controller.startDrawingGesture(toolName);
            });
        }
        return action;
    };

    // Toggle-to-draw actions
    setupGlobalAction(QStringLiteral("draw_freehand"), QStringLiteral("Draw Freehand"), 
                      QKeySequence(QStringLiteral("Meta+Shift+F")), QStringLiteral("freehand"));
    setupGlobalAction(QStringLiteral("draw_arrow"), QStringLiteral("Draw Arrow"), 
                      QKeySequence(QStringLiteral("Meta+Shift+A")), QStringLiteral("arrow"));
    setupGlobalAction(QStringLiteral("draw_rectangle"), QStringLiteral("Draw Rectangle"), 
                      QKeySequence(QStringLiteral("Meta+Shift+V")), QStringLiteral("rectangle"));
    setupGlobalAction(QStringLiteral("draw_ellipse"), QStringLiteral("Draw Ellipse"), 
                      QKeySequence(QStringLiteral("Meta+Shift+E")), QStringLiteral("ellipse"));
    setupGlobalAction(QStringLiteral("draw_line"), QStringLiteral("Draw Line"), 
                      QKeySequence(QStringLiteral("Meta+Shift+L")), QStringLiteral("line"));
    setupGlobalAction(QStringLiteral("draw_text"), QStringLiteral("Draw Text"), 
                      QKeySequence(QStringLiteral("Meta+Shift+T")), QStringLiteral("text"));

    // Trigger actions
    QAction *actionUndo = setupGlobalAction(QStringLiteral("action_undo"), QStringLiteral("Undo Last Action"), 
                                            QKeySequence(QStringLiteral("Meta+Ctrl+Z")));
    QAction *actionClear = setupGlobalAction(QStringLiteral("action_clear"), QStringLiteral("Clear Screen"), 
                                             QKeySequence(QStringLiteral("Meta+Ctrl+Delete")));
    QAction *actionSelect = setupGlobalAction(QStringLiteral("action_select_mode"), QStringLiteral("Toggle Select Mode"), 
                                              QKeySequence(QStringLiteral("Meta+Shift+S")));

    QObject::connect(actionUndo, &QAction::triggered, &controller, &OverlayController::undo);
    QObject::connect(actionClear, &QAction::triggered, &controller, &OverlayController::clear);
    QObject::connect(actionSelect, &QAction::triggered, &controller, [&controller]() {
        if (controller.currentMode() == QStringLiteral("select")) {
            controller.enterPassthroughMode();
        } else {
            controller.enterSelectMode();
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
