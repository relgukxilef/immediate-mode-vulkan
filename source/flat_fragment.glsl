#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec2 vertex_texture_coordinate;
layout(location = 1) in vec3 vertex_color;

layout(location = 0) out vec4 fragment_color;

void main() {
    fragment_color = vec4(vertex_color, 1);
    //fragment_color *= texture(source_texture, vertex_texture_coordinate);
}
