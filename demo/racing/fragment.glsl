#version 450
#pragma shader_stage(fragment)

layout (std140, binding = 0) uniform uniforms_t {
    mat4 matrix;
    vec4 color;
    vec4 velocity;
} uniforms;

layout(location = 0) in vec2 vertex_source;
layout(location = 1) in vec3 vertex_color;

layout(location = 0) out vec4 fragment_color;

void main() {
    vec2 tile = fract(vertex_source / 20);
    tile = abs(tile - 0.5) - 0.25;
    tile = clamp(tile / (fwidth(tile) + uniforms.velocity.xy / 20) * 2, -1, 1);
    float color = tile.x * tile.y;
    color = 0.9 + 0.1 * color;
    fragment_color = vec4(pow(color, (1 / 2.2)) * vertex_color.rgb, 1);
}
