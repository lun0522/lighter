#version 460 core

layout(binding = 1) uniform sampler2D diff_sampler;
layout(binding = 2) uniform sampler2D spec_sampler;
layout(binding = 3) uniform sampler2D refl_sampler;

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = vec4(1.0);
}
