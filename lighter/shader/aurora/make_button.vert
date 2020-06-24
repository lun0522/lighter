#version 460 core

#define NUM_VERTICES_PER_BUTTON 6

#if defined(TARGET_OPENGL)

layout(binding = 0) uniform VerticesInfo {
  vec4 pos_tex_coords[NUM_VERTICES_PER_BUTTON];
} vertices_info;

#elif defined(TARGET_VULKAN)

layout(push_constant) uniform VerticesInfo {
  vec4 pos_tex_coords[NUM_VERTICES_PER_BUTTON];
} vertices_info;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 center;

layout(location = 0) out vec3 color;
layout(location = 1) out vec2 tex_coord;

void main() {
  gl_Position = vec4(center + vertices_info.pos_tex_coords[gl_VertexIndex].xy,
                     0.0, 1.0);
  color = in_color;
  tex_coord = vertices_info.pos_tex_coords[gl_VertexIndex].zw;
}
