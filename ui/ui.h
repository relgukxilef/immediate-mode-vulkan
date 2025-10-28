#pragma once

#include "draw.h"

#include <memory>
#include <vector>

struct ui {
    ui() = default;
    ui(
        VkInstance instance, VkPhysicalDevice physical_device, 
        VkSurfaceKHR surface
    );

    void render();

    renderer renderer;

    float time = 0;
};
