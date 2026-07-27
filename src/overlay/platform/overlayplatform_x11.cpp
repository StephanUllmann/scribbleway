#include "overlayplatform.h"

#if defined(Q_OS_LINUX)
#include <QGuiApplication>
#include <QScreen>

class OverlayPlatformX11 : public OverlayPlatform {
public:
    void setupOverlay(QQuickWindow *window) override {
        if (!window) return;
        window->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::Tool);
        window->setColor(Qt::transparent);
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            window->setGeometry(screen->geometry());
        }
    }

    void setInputPassthrough(QQuickWindow *window, bool passthrough) override {
        if (!window) return;
        if (passthrough) {
            window->setMask(QRegion());
        } else {
            window->setMask(QRegion(0, 0, window->width(), window->height()));
        }
    }

    void setKeyboardInteractivity(QQuickWindow *window, bool interactive) override {
        if (!window) return;
        if (interactive) {
            window->requestActivate();
        }
    }
};

std::unique_ptr<OverlayPlatform> createX11Platform() {
    return std::make_unique<OverlayPlatformX11>();
}
#endif
