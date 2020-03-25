#version 460 core

#if defined(TARGET_OPENGL)
layout(binding = 0) uniform Transformation {
  mat4 model;
  mat4 proj_view;
} trans;

#elif defined(TARGET_VULKAN)
layout(push_constant) uniform Transformation {
  mat4 model;
  mat4 proj_view;
} trans;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_norm;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 norm;
layout(location = 1) out vec2 tex_coord;

void main() {
  const vec4 pos_world = trans.model * vec4(in_pos, 1.0);
  norm = normalize(pos_world.xyz);
  gl_Position = trans.proj_view * pos_world;
  tex_coord = in_tex_coord;
}
