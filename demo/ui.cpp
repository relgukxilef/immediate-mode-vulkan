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

    for (auto i = 0u; i < 1000; i++) {
        imv::draw({
            .stages = {
                { 
                    .codeFileName = "demo/video_vertex.glsl.spv",
                    .info = { .stage = VK_SHADER_STAGE_VERTEX_BIT, }
                }, { 
                    .codeFileName = "demo/video_fragment.glsl.spv",
                    .info = { .stage = VK_SHADER_STAGE_FRAGMENT_BIT, }
                }, 
            },
            .copyMemoryToUniformBufferSrcHostPointer = &uniforms,
            .copyMemoryToUniformBufferSize = sizeof(uniforms),
            .vertexCount = 6,
        });
        
        uniforms.time += 3.14f / 50;
    }

    imv::submit();
}
