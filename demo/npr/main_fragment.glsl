#version 450
#pragma shader_stage(fragment)

layout (std140, binding = 0) uniform uniforms_t {
    mat4 matrix;
} uniforms;

layout(location = 0) in vec3 vertex_color;
layout(location = 1) in noperspective vec3 coordinates;

layout(location = 0) out vec4 fragment_color;

void main() {
    fragment_color = vec4(1, vec2(step(vertex_color.r, 0.1)), 1);
    float d = min(min(coordinates.x, coordinates.y), coordinates.z);
    d = clamp(d / fwidth(d) - 1, 0, 1);
    fragment_color = vec4(vec3(d), 1);
}
