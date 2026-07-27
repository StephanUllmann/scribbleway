#include "overlayplatform.h"

#if defined(Q_OS_LINUX) && defined(HAVE_LAYERSHELLQT)
#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>

class OverlayPlatformWayland : public OverlayPlatform {
public:
    void setupOverlay(QQuickWindow *window) override {
        if (auto *layerWindow = LayerShellQt::Window::get(window)) {
            LayerShellQt::Window::Anchors anchors;
            anchors.setFlag(LayerShellQt::Window::AnchorTop);
            anchors.setFlag(LayerShellQt::Window::AnchorBottom);
            anchors.setFlag(LayerShellQt::Window::AnchorLeft);
            anchors.setFlag(LayerShellQt::Window::AnchorRight);
            layerWindow->setAnchors(anchors);
            layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
            layerWindow->setExclusiveZone(-1);
            layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
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
        if (auto *layerWindow = LayerShellQt::Window::get(window)) {
            layerWindow->setKeyboardInteractivity(interactive ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                                                              : LayerShellQt::Window::KeyboardInteractivityNone);
        }
    }
};

std::unique_ptr<OverlayPlatform> createWaylandPlatform() {
    return std::make_unique<OverlayPlatformWayland>();
}
#endif
