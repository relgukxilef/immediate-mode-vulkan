#pragma once

#include "draw.h"

#include <memory>
#include <vector>

struct ui {
    ui() = default;

    void render();

    float time = 0;
};
