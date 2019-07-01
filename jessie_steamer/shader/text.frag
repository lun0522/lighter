#version 460 core

layout(binding = 0) uniform CharInfo {
  vec4 color_alpha;
} char_info;

layout(binding = 1) uniform sampler2D tex_sampler;

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = vec4(char_info.color_alpha.rgb *
                    texture(tex_sampler, tex_coord).r,
                    char_info.color_alpha.a);
}
