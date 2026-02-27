#version 450
#pragma shader_stage(vertex)

layout (std140, binding = 0) uniform uniforms_t {
    vec2 position;
    float time;
    float window_width;
} uniforms;

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texture_coordinate;
layout (location = 2) in vec3 color;

layout(location = 0) out vec2 vertex_source;
layout(location = 1) out vec3 vertex_color;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    vertex_source = texture_coordinate * vec2(uniforms.window_width, 1);
    vertex_source -= uniforms.position;;
    vertex_color = color;
}
