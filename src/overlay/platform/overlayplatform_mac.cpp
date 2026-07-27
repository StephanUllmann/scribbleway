#include "overlayplatform.h"

#if defined(Q_OS_MACOS)
class OverlayPlatformMac : public OverlayPlatform {
public:
    void setupOverlay(QQuickWindow *window) override {
        if (!window) return;
        window->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        window->setColor(Qt::transparent);
        // Native NSWindow settings (level = .screenSaver, isOpaque = false) can be wired via Objective-C++
    }

    void setInputPassthrough(QQuickWindow *window, bool passthrough) override {
        if (!window) return;
        // Native NSWindow.ignoresMouseEvents = passthrough can be wired via Objective-C++
    }

    void setKeyboardInteractivity(QQuickWindow *window, bool interactive) override {
        if (!window) return;
        if (interactive) {
            window->requestActivate();
        }
    }
};

std::unique_ptr<OverlayPlatform> createMacPlatform() {
    return std::make_unique<OverlayPlatformMac>();
}
#endif
