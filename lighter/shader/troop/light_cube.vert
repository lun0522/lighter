#version 460 core

#define NUM_LIGHTS 32

layout(std140, binding = 0) uniform Lights {
  vec4 colors[NUM_LIGHTS];
} lights;

layout(std140, binding = 1) uniform RenderInfo {
  vec4 light_centers[NUM_LIGHTS];
  vec4 camera_pos;
} render_info;

#if defined(TARGET_OPENGL)
layout(std140, binding = 2) uniform Transformation {
  mat4 model;
  mat4 proj_view;
} trans;

#elif defined(TARGET_VULKAN)
layout(std140, push_constant) uniform Transformation {
  mat4 model;
  mat4 proj_view;
} trans;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(location = 0) in vec3 in_pos;

layout(location = 0) out vec3 color;

void main() {
  const vec3 pos_world = (trans.model * vec4(in_pos, 1.0)).xyz +
                         render_info.light_centers[gl_InstanceIndex].xyz;
  gl_Position = trans.proj_view * vec4(pos_world, 1.0);
  color = lights.colors[gl_InstanceIndex].xyz;
}
