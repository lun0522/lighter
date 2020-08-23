#version 460 core

layout(std140, binding = 1) uniform Light {
  vec4 direction_time;
} light;

layout(binding = 2) uniform sampler2D tex_sampler;

layout(location = 0) in vec3 norm;
layout(location = 1) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  const vec3 direction = normalize(-light.direction_time.xyz);
  const float diffuse = max(dot(norm, direction), 0.0);
  frag_color = diffuse * texture(tex_sampler, tex_coord);
}
