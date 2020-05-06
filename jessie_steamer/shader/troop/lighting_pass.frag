#version 460 core

layout(binding = 1) uniform sampler2D pos_sampler;
layout(binding = 2) uniform sampler2D norm_sampler;
layout(binding = 3) uniform sampler2D diff_spec_sampler;

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = texture(pos_sampler, tex_coord);
  frag_color = texture(diff_spec_sampler, tex_coord);
  frag_color = texture(norm_sampler, tex_coord);
}
