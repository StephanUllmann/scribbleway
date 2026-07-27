#include "overlayplatform.h"

#if defined(Q_OS_WIN)
#include <windows.h>

class OverlayPlatformWin : public OverlayPlatform {
public:
    void setupOverlay(QQuickWindow *window) override {
        if (!window) return;
        window->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        window->setColor(Qt::transparent);
        
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    }

    void setInputPassthrough(QQuickWindow *window, bool passthrough) override {
        if (!window) return;
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        if (passthrough) {
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
        } else {
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
        }
    }

    void setKeyboardInteractivity(QQuickWindow *window, bool interactive) override {
        if (!window) return;
        if (interactive) {
            window->requestActivate();
        }
    }
};

std::unique_ptr<OverlayPlatform> createWinPlatform() {
    return std::make_unique<OverlayPlatformWin>();
}
#endif
