#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec2 vertex_source;

layout(location = 0) out vec4 fragment_color;

void main() {
    fragment_color = vec4(1.0);
}
