#ifndef OVERLAYPLATFORM_H
#define OVERLAYPLATFORM_H

#include <memory>
#include <QQuickWindow>

class OverlayPlatform {
public:
    virtual ~OverlayPlatform() = default;

    virtual void setupOverlay(QQuickWindow *window) = 0;
    virtual void setInputPassthrough(QQuickWindow *window, bool passthrough) = 0;
    virtual void setKeyboardInteractivity(QQuickWindow *window, bool interactive) = 0;

    static std::unique_ptr<OverlayPlatform> create();
};

#endif // OVERLAYPLATFORM_H
