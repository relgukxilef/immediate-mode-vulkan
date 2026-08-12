#version 450
#pragma shader_stage(vertex)

layout (std140, binding = 0) uniform uniforms_t {
    mat4 matrix;
} uniforms;

layout(location = 0) in vec4 position;

layout(location = 0) out vec3 vertex_color;

void main() {
    gl_Position = uniforms.matrix * vec4(position.xyz, 1.0);
    vertex_color = vec3(position.xyz);
}
