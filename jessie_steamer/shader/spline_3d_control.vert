#version 460 core

#if defined(TARGET_OPENGL)
layout(binding = 0) uniform Transformation {
  mat4 proj_view_model;
} trans;

#elif defined(TARGET_VULKAN)
layout(push_constant) uniform Transformation {
  mat4 proj_view_model;
} trans;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(location = 0) in vec3 in_center;
layout(location = 1) in vec3 in_pos;
layout(location = 2) in vec4 in_color_alpha;

layout(location = 0) out vec4 color_alpha;

void main() {
  // TODO: pos and color_alpha
  gl_Position = trans.proj_view_model * vec4(in_center + in_pos * 0.01, 1.0);
  color_alpha = in_color_alpha;
  color_alpha = vec4(1.0, 0.0, 0.0, 1.0);
}
