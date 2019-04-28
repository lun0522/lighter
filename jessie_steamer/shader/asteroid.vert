#version 460 core

layout(binding = 0) uniform Transformation {
  mat4 model;
  mat4 view;
  mat4 proj;
} trans;

layout(binding = 1) uniform Light {
  vec4 direction_time;
} light;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_norm;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in float theta;
layout(location = 4) in float radius;
layout(location = 5) in mat4 model;

layout(location = 0) out vec3 norm;
layout(location = 1) out vec2 tex_coord;

void main() {
  float angle = theta + light.direction_time.w * 0.1;
  vec3 center = vec3(sin(angle) * radius, 0.0, cos(angle) * radius);
  vec4 pos_world = model * vec4(in_pos, 1.0);
  norm = normalize(pos_world.xyz);
  gl_Position = trans.proj * trans.view *
                (pos_world + vec4(center * pos_world.w, 0.0));
  tex_coord = in_tex_coord;
}