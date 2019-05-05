#version 460 core

#ifdef TARGET_OPENGL
layout(binding = 0) uniform Transformation {
  mat4 proj;
  mat4 view;
} trans;
#endif

#ifdef TARGET_VULKAN
layout(push_constant) uniform Transformation {
  mat4 proj;
  mat4 view;
} trans;
#endif

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_norm;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 tex_coord;

void main() {
  // ignore translation, so that camera never moves relative to skybox
  gl_Position = trans.proj * mat4(mat3(trans.view)) * vec4(in_pos, 1.0);
  gl_Position.zw = vec2(1.0);
  tex_coord = in_pos;
}
