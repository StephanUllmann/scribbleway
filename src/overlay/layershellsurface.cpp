#include "layershellsurface.h"

#include <LayerShellQt/Window>

void LayerShellSurface::apply(QWindow *window, const LayerConfig &config)
{
    using W = LayerShellQt::Window;
    W *layer = W::get(window);
    if (!layer) return;

    W::Anchors anchors;
    anchors.setFlag(W::AnchorTop, config.anchors.testFlag(Qt::TopEdge));
    anchors.setFlag(W::AnchorBottom, config.anchors.testFlag(Qt::BottomEdge));
    anchors.setFlag(W::AnchorLeft, config.anchors.testFlag(Qt::LeftEdge));
    anchors.setFlag(W::AnchorRight, config.anchors.testFlag(Qt::RightEdge));
    const W::KeyboardInteractivity keyboard =
        config.keyboard == LayerConfig::Keyboard::Exclusive  ? W::KeyboardInteractivityExclusive
        : config.keyboard == LayerConfig::Keyboard::OnDemand ? W::KeyboardInteractivityOnDemand
                                                             : W::KeyboardInteractivityNone;

    // Only what differs: each setter re-commits the surface, and the keyboard mode changes
    // every time a tool is picked up or put down.
    if (layer->anchors() != anchors) layer->setAnchors(anchors);
    if (layer->margins() != config.margins) layer->setMargins(config.margins);
    // Both of Scribbleway's surfaces sit above everything, panels included.
    if (layer->layer() != W::LayerOverlay) layer->setLayer(W::LayerOverlay);
    if (layer->exclusionZone() != config.exclusiveZone) layer->setExclusiveZone(config.exclusiveZone);
    if (layer->keyboardInteractivity() != keyboard) layer->setKeyboardInteractivity(keyboard);
    if (layer->scope() != config.scope) layer->setScope(config.scope);
    // Qt-Wayland ignores QWindow::setScreen() (the compositor owns placement), so a surface
    // that should follow the user asks the compositor to map it on the active output.
    // setScreenConfiguration() is the pre-6.6 spelling of this.
#ifdef HAVE_LAYERSHELL_ACTIVE_SCREEN
    if (layer->wantsToBeOnActiveScreen() != config.onActiveScreen) {
        layer->setWantsToBeOnActiveScreen(config.onActiveScreen);
    }
#else
    const auto screen = config.onActiveScreen ? W::ScreenFromCompositor : W::ScreenFromQWindow;
    if (layer->screenConfiguration() != screen) layer->setScreenConfiguration(screen);
#endif
}
