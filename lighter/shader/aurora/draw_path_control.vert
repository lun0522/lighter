#version 460 core

#if defined(TARGET_OPENGL)
layout(std140, binding = 0) uniform RenderInfo {
  mat4 proj_view_model;
  vec4 color_alpha;
  float scale;
} render_info;

#elif defined(TARGET_VULKAN)
layout(std140, push_constant) uniform RenderInfo {
  mat4 proj_view_model;
  vec4 color_alpha;
  float scale;
} render_info;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(location = 0) in vec3 in_center;
layout(location = 1) in vec3 in_pos;

layout(location = 0) out vec4 color_alpha;

void main() {
  gl_Position = render_info.proj_view_model *
                vec4(in_center + in_pos * render_info.scale, 1.0);
  color_alpha = render_info.color_alpha;
}
