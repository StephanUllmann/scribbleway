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
#include <QDebug>
#include <QDir>

#include <LayerShellQt/Shell>
#include <KGlobalAccel>

#include <signal.h>

#include "devlog.h"
#include "layershellsurface.h"
#include "overlaycontroller.h"
#include "overlaywindow.h"
#include "traypopup.h"
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
    OverlayController controller;

    // Register global hotkeys from the same table as the overlay-local ones.
    for (const HotkeySpec &h : OverlayController::hotkeys()) {
        const bool isGlobal = h.scope == HotkeySpec::Scope::Global
            || (devMode && h.scope == HotkeySpec::Scope::GlobalDev);
        if (!isGlobal) continue;

        auto *action = new QAction(&app);
        action->setObjectName(QString::fromLatin1(h.id));
        action->setText(QString::fromUtf8(h.label));
        const QList<QKeySequence> seqs = {QKeySequence(QString::fromUtf8(h.defaultSeq))};
        KGlobalAccel::self()->setDefaultShortcut(action, seqs);
        KGlobalAccel::self()->setShortcut(action, seqs);
        const QString id = QString::fromLatin1(h.id);
        QObject::connect(action, &QAction::triggered, &controller, [&controller, id]() {
            controller.triggerShortcut(id);
        });
    }

    // KGlobalAccel is inert outside Plasma, so `scribbleway-overlay --toggle` re-uses the
    // single-instance socket as the entry point other compositors can bind:
    //   bind = SUPER SHIFT, X, exec, scribbleway-overlay --toggle
    QObject::connect(instanceGuard.server(), &QLocalServer::newConnection, &app, [&instanceGuard, &controller]() {
        QLocalSocket *client = instanceGuard.server()->nextPendingConnection();
        if (!client) return;
        QObject::connect(client, &QLocalSocket::readyRead, client, [client, &controller]() {
            const QByteArray request = client->readAll();
            if (request.startsWith("toggle")) {
                controller.toggleMode();
            } else if (request.startsWith("mark")) {
                DevLog::mark();
            }
        });
        QObject::connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    });
    LayerShellSurface layerShell;
    OverlayWindow overlayWindow(&layerShell);
    TrayPopup trayPopup(&layerShell, &overlayWindow);

    // Load QML Engine
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("overlayWindow"), &overlayWindow);
    engine.rootContext()->setContextProperty(QStringLiteral("trayPopup"), &trayPopup);
    engine.rootContext()->setContextProperty(QStringLiteral("devMode"), devMode);
    
    const QUrl url(QStringLiteral("qrc:/scribbleway/qml/main.qml"));
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, &controller, &overlayWindow](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
            return;
        }

        if (objUrl != url) {
            return; // Only the overlay window gets the fullscreen layer-shell treatment.
        }

        if (auto *window = qobject_cast<QQuickWindow*>(obj)) {
            overlayWindow.attach(window);
            controller.setWindow(window);
        }
    }, Qt::QueuedConnection);

    engine.load(url);

    // A small corner-anchored layer-shell surface, hidden until the tray icon is clicked.
    const QUrl trayPopupUrl(QStringLiteral("qrc:/scribbleway/qml/TrayPopup.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [trayPopupUrl, &trayPopup](QObject *obj, const QUrl &objUrl) {
        if (objUrl != trayPopupUrl) return;
        if (auto *window = qobject_cast<QQuickWindow*>(obj)) trayPopup.attach(window);
    }, Qt::QueuedConnection);
    engine.load(trayPopupUrl);

    QSystemTrayIcon trayIcon(QIcon::fromTheme(QStringLiteral("draw-freehand"),
                                              QIcon(QStringLiteral(":/icons/draw-freehand.svg"))));
    trayIcon.setToolTip(QStringLiteral("Scribbleway"));
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, &trayPopup,
                     [&trayPopup](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) trayPopup.toggle();
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
