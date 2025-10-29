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

    struct {
        float time;
    } uniforms;

    uniforms.time = time;

    draw({
        .renderer = &renderer,
        .stages = {
            { 
                .codeFileName = "ui/video_vertex.glsl.spv",
                .info = { .stage = VK_SHADER_STAGE_VERTEX_BIT, }
            }, { 
                .codeFileName = "ui/video_fragment.glsl.spv",
                .info = { .stage = VK_SHADER_STAGE_FRAGMENT_BIT, }
            }, 
        },
        .copyMemoryToUniformBufferSrcHostPointer = &uniforms,
        .copyMemoryToUniformBufferSize = sizeof(uniforms),
        .vertexCount = 6,
    });
    
    submit(&renderer);
}
