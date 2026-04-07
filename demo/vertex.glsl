#version 450
#pragma shader_stage(vertex)

layout (std140, binding = 0) uniform uniforms_t {
    mat4 matrix;
    vec4 color;
    vec4 velocity;
} uniforms;

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texture_coordinate;

layout(location = 0) out vec2 vertex_source;
layout(location = 1) out vec3 vertex_color;

void main() {
    gl_Position = uniforms.matrix * vec4(position, 0.0, 1.0);
    vertex_source = texture_coordinate;
    vertex_color = uniforms.color.rgb;
}
