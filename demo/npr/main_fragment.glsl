#version 450
#pragma shader_stage(fragment)

layout (std140, binding = 0) uniform uniforms_t {
    mat4 matrix;
} uniforms;

layout(location = 0) in vec3 vertex_color;

layout(location = 0) out vec4 fragment_color;

void main() {
    fragment_color = vec4(vertex_color.rgb, 1);
}
