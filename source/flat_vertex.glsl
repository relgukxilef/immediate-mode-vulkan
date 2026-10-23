#version 450
#pragma shader_stage(vertex)

layout (std140, binding = 0) uniform parameters_t {
    vec2 position;
    vec2 scale;
} parameters;

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texture_coordinate;
layout (location = 2) in vec3 color;

layout(location = 0) out vec2 vertex_texture_coordinate;
layout(location = 1) out vec3 vertex_color;

void main() {
    gl_Position = vec4(position * parameters.scale + parameters.position, 0, 1);
    vertex_texture_coordinate = texture_coordinate;
    vertex_color = color;
}
