#version 460 core

#define NUM_VERTICES_PER_BUTTON 6

#if defined(TARGET_OPENGL)

#define MIN_DEPTH -1.0

layout(binding = 0) uniform VerticesInfo {
  vec4 pos_tex_coords[NUM_VERTICES_PER_BUTTON];
} vertices_info;

#elif defined(TARGET_VULKAN)

#define MIN_DEPTH  0.0

layout(push_constant) uniform VerticesInfo {
  vec4 pos_tex_coords[NUM_VERTICES_PER_BUTTON];
} vertices_info;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(location = 0) in vec4 in_color_alpha;
layout(location = 1) in vec2 center;

layout(location = 0) out vec4 color_alpha;
layout(location = 1) out vec2 tex_coord;

void main() {
  gl_Position = vec4(center + vertices_info.pos_tex_coords[gl_VertexIndex].xy,
                     MIN_DEPTH, 1.0);
  color_alpha = in_color_alpha;
  tex_coord = vertices_info.pos_tex_coords[gl_VertexIndex].zw;
}
