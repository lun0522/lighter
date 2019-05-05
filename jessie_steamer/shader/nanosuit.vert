#version 460 core

#ifdef TARGET_OPENGL
layout(binding = 0) uniform Transformation {
  mat4 view_model;
  mat4 proj_view_model;
  mat4 view_model_inv_trs;
  mat4 view_inv;
} trans;
#endif

#ifdef TARGET_VULKAN
layout(push_constant) uniform Transformation {
  mat4 view_model;
  mat4 proj_view_model;
  mat4 view_model_inv_trs;
  mat4 view_inv;
} trans;
#endif

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_norm;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 pos_view;
layout(location = 1) out vec3 norm_view;
layout(location = 2) out vec2 tex_coord;

void main() {
  gl_Position = trans.proj_view_model * vec4(in_pos, 1.0);
  pos_view = (trans.view_model * vec4(in_pos, 1.0)).xyz;
  norm_view = (trans.view_model_inv_trs * vec4(in_norm, 0.0)).xyz;
  norm_view = normalize(norm_view);
  tex_coord = in_tex_coord;
}
