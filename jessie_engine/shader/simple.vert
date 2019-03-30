#version 450 core

layout(binding = 0) uniform Transformation {
  mat4 model;
  mat4 view;
  mat4 proj;
} trans;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_norm;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec2 tex_coord;

void main() {
  gl_Position = trans.proj * trans.view * trans.model * vec4(in_pos, 1.0);
  tex_coord = in_tex_coord;
}
