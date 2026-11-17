#pragma once

#include "layersurface.h"

class LayerShellSurface : public LayerSurface
{
public:
    void apply(QWindow *window, const LayerConfig &config) override;
};
