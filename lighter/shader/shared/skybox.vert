#version 460 core

#if defined(TARGET_OPENGL)
layout(std140, binding = 0) uniform Transformation {
  mat4 proj_view_model;
} trans;

#elif defined(TARGET_VULKAN)
layout(std140, push_constant) uniform Transformation {
  mat4 proj_view_model;
} trans;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_norm;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 tex_coord;

void main() {
  // Ignore translation, so that camera never moves relative to skybox.
  gl_Position = trans.proj_view_model * vec4(in_pos, 1.0);
  gl_Position.zw = vec2(1.0);
  tex_coord = in_pos;
}
