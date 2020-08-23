#version 460 core

#if defined(TARGET_OPENGL)
layout(std140, binding = 0) uniform RenderInfo {
  mat4 proj_view_model;
  vec4 color_alpha;
  vec4 center_scale;
} render_info;

#elif defined(TARGET_VULKAN)
layout(std140, push_constant) uniform RenderInfo {
  mat4 proj_view_model;
  vec4 color_alpha;
  vec4 center_scale;
} render_info;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(location = 0) in vec3 in_pos;

layout(location = 0) out vec4 color_alpha;

void main() {
  const vec3 center = render_info.center_scale.xyz;
  const float scale = render_info.center_scale.w;
  gl_Position = render_info.proj_view_model *
                vec4(center + in_pos * scale, 1.0);
  color_alpha = render_info.color_alpha;
}
