#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D tex_sampler;

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = vec4(color * texture(tex_sampler, tex_coord).rgb, 1.0);
}
