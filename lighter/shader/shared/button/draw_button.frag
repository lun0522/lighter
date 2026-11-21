#version 460 core

layout(binding = 1) uniform sampler2D tex_sampler;

layout(location = 0) in float alpha;
layout(location = 1) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = texture(tex_sampler, tex_coord);
  frag_color.a *= alpha;
}
