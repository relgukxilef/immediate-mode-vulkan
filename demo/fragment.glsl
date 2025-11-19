#version 450
#pragma shader_stage(fragment)

layout(binding = 1) uniform sampler2D source_texture;
layout(binding = 2) uniform sampler2D source_texture_b;

layout(location = 0) in vec2 vertex_source;

layout(location = 0) out vec4 fragment_color;

void main() {
    fragment_color = texture(source_texture, vertex_source);
    fragment_color *= texture(source_texture_b, vertex_source);
}
