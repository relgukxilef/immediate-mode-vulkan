#version 450
#pragma shader_stage(vertex)

layout (std140, binding = 0) uniform parameters {
    float time;
};

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texture_coordinate;
layout (location = 2) in vec3 color;

layout(location = 0) out vec2 vertex_source;
layout(location = 1) out vec3 vertex_color;

void main() {
    gl_Position = vec4(
        position * 0.1 + sin(0.1 * time * vec2(1, sqrt(2))),
        0.0, 1.0
    );
    vertex_source = texture_coordinate;
    vertex_color = color;
}
