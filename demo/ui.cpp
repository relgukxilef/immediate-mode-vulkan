#include "ui.h"

#include <vector>
#include <memory>
#include <algorithm>

using namespace std;

void ui::render() {
    imv::wait_frame();

    struct {
        float time;
    } uniforms;

    uniforms.time = time;

    imv::draw({
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

    imv::draw({
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
    
    imv::submit();
}
