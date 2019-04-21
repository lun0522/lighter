#version 460 core

layout(binding = 1) uniform sampler2D diff_samplers[];
layout(binding = 2) uniform sampler2D spec_samplers[];
layout(binding = 3) uniform sampler2D refl_samplers[];

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

// TODO: use push constants to select texture
void main() {
  frag_color = texture(diff_sampler, tex_coord) +
               texture(spec_sampler, tex_coord) * 0.3f;
}
