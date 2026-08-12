#version 450
#pragma shader_stage(fragment)

layout (std140, binding = 0) uniform uniforms_t {
    mat4 matrix;
    vec4 color;
    vec4 velocity;
} uniforms;

layout(binding = 1) uniform sampler2D font;

layout(location = 0) in vec2 vertex_source;
layout(location = 1) in vec3 vertex_color;

layout(location = 0) out vec4 fragment_color;

void main() {
    float distance = texture(font, vertex_source).r;
    if (distance < 0.25)
        discard;
    distance = distance - 0.5;
    vec2 derivative = vec2(dFdx(distance), dFdy(distance));
    vec3 distance2 = vec3(distance - derivative.x / 3, distance, distance + derivative.x / 3);
    distance2 = clamp(distance2 / (abs(derivative.x) + abs(derivative.y)), -0.5, 0.5) + 0.5;
    fragment_color = vec4(pow(distance2, vec3(1 / 2.2)), 1);
}
