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

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_color_alpha;

layout(location = 0) out vec4 color_alpha;

void main() {
  gl_Position = trans.proj_view_model * vec4(in_pos, 1.0);
  color_alpha = in_color_alpha;
}
