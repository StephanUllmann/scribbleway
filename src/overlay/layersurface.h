#pragma once

#include <QMargins>
#include <QString>
#include <QtGlobal>

class QWindow;

// How a layer-shell surface asks the compositor to place it. A plain value, so the code
// that decides it can be tested without LayerShellQt.
struct LayerConfig {
    enum class Keyboard { None, Exclusive, OnDemand };

    Qt::Edges anchors;
    QMargins margins;
    int exclusiveZone = 0;
    Keyboard keyboard = Keyboard::None;
    QString scope;
    // Let the compositor pick the output, as it does for the Tray Popup.
    bool onActiveScreen = false;

    bool operator==(const LayerConfig &) const = default;
};

// Where a layer-shell config reaches the window. LayerShellSurface is the real one; the
// tests record what they are given instead.
class LayerSurface
{
public:
    virtual ~LayerSurface() = default;
    virtual void apply(QWindow *window, const LayerConfig &config) = 0;
};
