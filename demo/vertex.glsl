#version 450
#pragma shader_stage(vertex)

layout (std140, binding = 0) uniform parameters {
    float time;
};

layout(location = 0) out vec2 vertex_source;

vec4 positions[] = vec4[](
    vec4(-1, -1, 0, 0),
    vec4(1, -1, 0, 0),
    vec4(-1, 1, 0, 0),
    vec4(1, 1, 0, 0)
);

void main() {
    vec2 position = positions[gl_VertexIndex].xy * 0.1;
    position = position + sin(0.1 * time * vec2(1, sqrt(2)));
    gl_Position = vec4(
        position,
        0.0, 1.0
    );
    vertex_source = positions[gl_VertexIndex].xy * 0.5 + 0.5;
}
