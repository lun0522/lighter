#version 460 core

layout(std140, binding = 0) uniform TransVert {
  mat4 view_model;
  mat4 proj_view_model;
  mat4 view_model_inv_trs;
} trans_vert;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_norm;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 pos_view;
layout(location = 1) out vec3 norm_view;
layout(location = 2) out vec2 tex_coord;

void main() {
  gl_Position = trans_vert.proj_view_model * vec4(in_pos, 1.0);
  pos_view = (trans_vert.view_model * vec4(in_pos, 1.0)).xyz;
  norm_view = (trans_vert.view_model_inv_trs * vec4(in_norm, 0.0)).xyz;
  norm_view = normalize(norm_view);
  tex_coord = in_tex_coord;
}
