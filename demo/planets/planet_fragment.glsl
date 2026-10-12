#version 450
#pragma shader_stage(fragment)
#extension GL_EXT_fragment_shader_barycentric : require

layout (std140, binding = 0) uniform uniforms_t {
    mat4 matrix;
} uniforms;

layout(location = 0) in vec3 vertex_color;

layout(location = 0) out vec4 fragment_color;

void main() {
    vec3 lines = step(1, (gl_BaryCoordNoPerspEXT) / fwidth(gl_BaryCoordNoPerspEXT));
    float line = lines.x * lines.y * lines.z;
    fragment_color = vec4(line, 1, 1, 1);
}
