#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec2 vertex_source;
layout(location = 1) in flat float line;

layout(location = 0) out vec4 fragment_color;

layout(binding = 0) uniform sampler2D source_texture;

void main() {
    fragment_color = vec4(texture(source_texture, vertex_source).rgb, 1.0);
    fragment_color = vec4(vec3(line * 0.5 + 0.5), 1.0);
}
