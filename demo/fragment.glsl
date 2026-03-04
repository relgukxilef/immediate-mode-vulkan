#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec2 vertex_source;
layout(location = 1) in vec3 vertex_color;

layout(location = 0) out vec4 fragment_color;

void main() {
    vec2 tile = fract(vertex_source * 0.5);
    tile = abs(tile - 0.5) - 0.25;
    float color = tile.x * tile.y;
    color = 0.5 + 0.5 * clamp(color / fwidth(color) * 2, -1, 1);
    fragment_color = vec4(color * vertex_color, 1);
}
