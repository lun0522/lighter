#version 460 core

#ifdef TARGET_OPENGL
layout(binding = 1) uniform Light {
  vec4 direction_time;
} light;
#endif

#ifdef TARGET_VULKAN
layout(push_constant) uniform Light {
  mat4 model;
  mat4 proj_view;
  vec4 direction_time;
} light;
#endif

layout(binding = 2) uniform sampler2D tex_sampler;

layout(location = 0) in vec3 norm;
layout(location = 1) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  vec3 direction = normalize(-light.direction_time.xyz);
  float diffuse = max(dot(norm, direction), 0.0);
  frag_color = diffuse * texture(tex_sampler, tex_coord);
}
