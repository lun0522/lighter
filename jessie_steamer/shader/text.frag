#version 460 core

layout(binding = 0) uniform TextRenderInfo {
  vec4 color_alpha;
} text_render_info;

layout(binding = 1) uniform sampler2D tex_sampler;

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = vec4(text_render_info.color_alpha.rgb *
                    texture(tex_sampler, tex_coord).r,
                    text_render_info.color_alpha.a);
}
