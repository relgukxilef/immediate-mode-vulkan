#version 450
#pragma shader_stage(vertex)

layout(location = 0) out vec2 vertex_source;
layout(location = 1) out flat float line;

vec4 positions[] = vec4[](
    vec4(-2, 1, 0, 0),
    vec4(0, 0, 0, 1),
    vec4(0, 2, 0, 1),
    vec4(0, 0, 0, 0),
    vec4(0, 2, 0, 0),
    vec4(2, 1, 0, 0)
);

// TODO: replace with neighbor indices, takes up less space
vec4 tangents[] = vec4[](
    vec4(2, 1, 2, -1),
    vec4(-2, 1, 0, 2),
    vec4(0, -2, -2, -1),
    vec4(0, 2, 2, 1),
    vec4(2, -1, 0, -2),
    vec4(-2, -1, -2, 1)
);

vec2 inset(vec2 a, vec2 b) {
    a = normalize(a);
    b = normalize(b);
    vec2 p = a + b;
    float inverse_thickness = dot(p, a.yx * vec2(1, -1));
    return p / inverse_thickness;
}

void main() {
    vec2 position = positions[gl_VertexIndex].xy * 0.1;
    vec4 tangent = tangents[gl_VertexIndex];
    position += inset(tangent.xy, tangent.zw) * 0.01;
    gl_Position = vec4(
        position,
        0.0, 1.0
    );
    vertex_source = positions[gl_VertexIndex].xy * vec2(1280, 720) / vec2(2048);
    line = positions[gl_VertexIndex].w;
}
