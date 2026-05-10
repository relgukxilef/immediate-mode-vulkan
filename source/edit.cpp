#include <memory>
#include <vector>
#include <immediate_mode_vulkan/edit.h>
#include <immediate_mode_vulkan/draw.h>
#include "glm/vector_relational.hpp"
#include "renderer.h"

using namespace std;
using namespace glm;

namespace imv {

    struct editor_data {
        vector<vec2> points;
        inputs previous_inputs{}, current_inputs{};
        size_t selected_point = 0;
        size_t points_size = 0;
    };

    editor::editor() {
        d = make_unique<editor_data>();
    }

    editor::~editor() {}

    editor* global_editor;

    void set_inputs(const inputs& i) {
        auto& e = *global_editor->d;
        e.previous_inputs = exchange(e.current_inputs, i);
        e.points_size = 0;
    }

    vec2 edit(vec2& position) {
        auto& e = *global_editor->d;
        vec2 scale = 2.f / vec2{ 
            get_surface_size().width, get_surface_size().height 
        };

        vec2 mouse = { e.current_inputs.mouse.x, e.current_inputs.mouse.y };
        mouse *= scale;
        mouse -= 1;

        if (e.current_inputs.mouse.primary) {
            if (!e.previous_inputs.mouse.primary) {
                if (all(lessThan(abs(position - mouse), 10.f * scale))) {
                    e.selected_point = e.points_size;
                }
            }
            if (e.selected_point == e.points_size) {
                position += (
                    vec2{ e.current_inputs.mouse.x, e.current_inputs.mouse.y } -
                    vec2{ e.previous_inputs.mouse.x, e.previous_inputs.mouse.y } 
                ) * scale;
            }
        } else {
            e.selected_point = -1;
        }

        e.points_size++;

        // TODO: draw widget on top of other elements
        // Either by deferring draw, drawing into separate command buffer,
        // drawing into separate frame buffer, or drawing with high z value
        // immediate is better than deferred
        vec2 positions[] = {
            {20, -1}, {20, 1}, {100, -1}, {100, 1}, 
            {100, -10}, {100, 10}, {130, 0},
            {130, 0}, {-1, 20}, 
            {-1, 20}, {1, 20}, {-1, 100}, {1, 100}, 
            {-10, 100}, {10, 100}, {0, 130}, 
        };
        vec2 texture_coordinate;
        vec3 x = {0.9, 0.2, 0.3}, y = {0.3, 0.9, 0.2};
        vec3 color[] = {
            x, x, x, x, x, x, x, x, 
            y, y, y, y, y, y, y, y, 
        };

        struct uniforms {
            vec2 position;
            vec2 scale;
        };

        imv::draw({
            .stages = {
                { 
                    // TODO: allow customizing path?
                    .code_file_name = "source/flat_vertex.glsl.spv",
                    .info = { .stage = VK_SHADER_STAGE_VERTEX_BIT, }
                }, { 
                    .code_file_name = "source/flat_fragment.glsl.spv",
                    .info = { .stage = VK_SHADER_STAGE_FRAGMENT_BIT, }
                }, 
            },
            .vertex_input_bindings = {
                {
                    .buffer_source_pointer = &positions,
                    .buffer_source_size = sizeof(positions),
                    .description = {
                        .stride = sizeof(vec2),
                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                    }, 
                    .attributes = {
                        { 0, 0, VK_FORMAT_R32G32_SFLOAT, },
                    },
                }, {
                    .buffer_source_pointer = &texture_coordinate,
                    .buffer_source_size = sizeof(texture_coordinate),
                    .description = {
                        .binding = 1,
                        .stride = 0,
                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                    }, 
                    .attributes = {
                        { 1, 1, VK_FORMAT_R32G32_SFLOAT, },
                    },
                }, {
                    .buffer_source_pointer = &color,
                    .buffer_source_size = sizeof(color),
                    .description = {
                        .binding = 2,
                        .stride = sizeof(vec3),
                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                    }, 
                    .attributes = {
                        { 2, 2, VK_FORMAT_R32G32B32_SFLOAT, },
                    },
                },
            },
            .uniform_source = uniforms{ 
                .position = position, 
                .scale = scale,
            },
            .vertex_count = size(positions),
        });

        return position;
    }

}
