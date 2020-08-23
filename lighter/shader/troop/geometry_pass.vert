#version 460 core

layout(std140, binding = 0) uniform Transformation {
  mat4 model;
  mat4 model_inv_trs;
  mat4 proj_view;
} trans;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_norm;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec3 in_center;

layout(location = 0) out vec3 pos;
layout(location = 1) out vec3 norm;
layout(location = 2) out vec2 tex_coord;

void main() {
  pos = (trans.model * vec4(in_pos, 1.0)).xyz + in_center;
  norm = mat3(trans.model_inv_trs) * in_norm;
  tex_coord = in_tex_coord;
  gl_Position = trans.proj_view * vec4(pos, 1.0);
}
