#include "ui.h"

#include <vector>
#include <memory>
#include <algorithm>

using namespace std;

void ui::render() {
    wait_frame();

    struct {
        float time;
    } uniforms;

    uniforms.time = time;

    draw({
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
    
    uniforms.time = time + 3.14f;

    draw({
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
    
    submit();
}
