#version 460 core

layout(binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out float frag_color;

void main() {
  frag_color = texture(tex_sampler, tex_coord).r;
}
