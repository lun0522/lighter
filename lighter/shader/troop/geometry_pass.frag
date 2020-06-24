#version 460 core

layout(binding = 1) uniform sampler2D diff_sampler;
layout(binding = 2) uniform sampler2D spec_sampler;
layout(binding = 3) uniform sampler2D refl_sampler;

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 tex_coord;

layout(location = 0) out vec3 frag_pos;
layout(location = 1) out vec3 frag_norm;
layout(location = 2) out vec4 diff_spec;

void main() {
  frag_pos = pos;
  frag_norm = normalize(norm);
  diff_spec = vec4(texture(diff_sampler, tex_coord).rgb,
                   texture(spec_sampler, tex_coord).r);
}
