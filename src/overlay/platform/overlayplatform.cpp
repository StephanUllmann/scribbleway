#include "overlayplatform.h"
#include <QGuiApplication>
#include <QDebug>

#if defined(Q_OS_LINUX)
#if defined(HAVE_LAYERSHELLQT)
std::unique_ptr<OverlayPlatform> createWaylandPlatform();
#endif
std::unique_ptr<OverlayPlatform> createX11Platform();
#elif defined(Q_OS_WIN)
std::unique_ptr<OverlayPlatform> createWinPlatform();
#elif defined(Q_OS_MACOS)
std::unique_ptr<OverlayPlatform> createMacPlatform();
#endif

std::unique_ptr<OverlayPlatform> OverlayPlatform::create() {
#if defined(Q_OS_LINUX)
    QString platformName = QGuiApplication::platformName();
    if (platformName.contains(QStringLiteral("wayland"), Qt::CaseInsensitive)) {
#if defined(HAVE_LAYERSHELLQT)
        qDebug() << "OverlayPlatform: Using Wayland LayerShellQt backend";
        return createWaylandPlatform();
#else
        qDebug() << "OverlayPlatform: Wayland detected, fallback to X11 platform wrapper";
        return createX11Platform();
#endif
    } else {
        qDebug() << "OverlayPlatform: Using X11 backend";
        return createX11Platform();
    }
#elif defined(Q_OS_WIN)
    qDebug() << "OverlayPlatform: Using Windows backend";
    return createWinPlatform();
#elif defined(Q_OS_MACOS)
    qDebug() << "OverlayPlatform: Using macOS backend";
    return createMacPlatform();
#else
    qWarning() << "OverlayPlatform: Unknown platform";
    return nullptr;
#endif
}
