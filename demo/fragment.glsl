#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec2 vertex_source;
layout(location = 1) in vec3 vertex_color;

layout(location = 0) out vec4 fragment_color;

void main() {
    vec2 tile = fract(vertex_source * 2) - 0.5;
    float color = 0.5 + 0.5 * sign(tile.x * tile.y);
    fragment_color = vec4(color * vertex_color, 1);
}
