#version 450
#pragma shader_stage(vertex)

layout (std140, binding = 0) uniform uniforms_t {
    mat4 matrix;
} uniforms;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 positions[3];

layout(location = 0) out vec3 vertex_color;
layout(location = 1) out vec3 coordinates;

void main() {
    gl_Position = uniforms.matrix * vec4(position.xyz, 1.0);
    vertex_color = vec3(position.w);
    int index = gl_VertexIndex % 3;
    coordinates = vec3(index == 0, index == 1, index == 2);
}
