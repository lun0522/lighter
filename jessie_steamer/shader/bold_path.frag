#version 460 core

#define SAMPLING_RADIUS 5

layout(binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out float frag_color;

void main() {
  const vec2 texel_size = 1.0 / textureSize(tex_sampler, 0);
  frag_color = 0.0;
  for (int x = -SAMPLING_RADIUS; x <= SAMPLING_RADIUS; ++x) {
    const float offset_x = texel_size.x * x;
    for (int y = -SAMPLING_RADIUS; y <= SAMPLING_RADIUS; ++y) {
      frag_color += texture(
          tex_sampler, tex_coord + vec2(offset_x, texel_size.y * y)).r;
    }
  }
}
