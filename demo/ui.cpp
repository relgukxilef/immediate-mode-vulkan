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
                    .code_file_name = "demo/vertex.glsl.spv",
                    .info = { .stage = VK_SHADER_STAGE_VERTEX_BIT, }
                }, { 
                    .code_file_name = "demo/fragment.glsl.spv",
                    .info = { .stage = VK_SHADER_STAGE_FRAGMENT_BIT, }
                }, 
            },
            .images = {
                {
                    .file_name = "demo/placeholder.ktx",
                    .sampler_info = {
                        .magFilter = VK_FILTER_LINEAR,
                        .minFilter = VK_FILTER_LINEAR,
                        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        .anisotropyEnable = VK_FALSE,
                        .minLod = 0.0,
                        .maxLod = VK_LOD_CLAMP_NONE,
                    }
                }
            },
            .uniform_source_pointer = &uniforms,
            .uniform_source_size = sizeof(uniforms),
            .vertex_count = 4,
        });
        
        uniforms.time += 0.5f;
    }

    imv::submit();
}
