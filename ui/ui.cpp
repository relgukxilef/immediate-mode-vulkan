#include "ui.h"

#include <vector>
#include <memory>
#include <algorithm>

using namespace std;

ui::ui(
    VkInstance instance, VkPhysicalDevice physical_device, VkSurfaceKHR surface
) : renderer(instance, physical_device, surface) {
}

void ui::render() {
    wait_frame(&renderer);

    draw({
        .renderer = &renderer,
        .shaderModuleCodeFileNames = {
            "ui/video_vertex.glsl.spv", "ui/video_fragment.glsl.spv",
        },
        .copyMemoryToUniformBufferSrcHostPointer = &time,
        .copyMemoryToUniformBufferSize = sizeof(float),
        .vertexCount = 6,
    });
    
    submit(&renderer);
}
