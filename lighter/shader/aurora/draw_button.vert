#version 460 core

#define NUM_VERTICES_PER_BUTTON 6

layout(std140, binding = 0) uniform VerticesInfo {
  vec4 pos_tex_coords[NUM_VERTICES_PER_BUTTON];
} vertices_info;

layout(location = 0) in float in_alpha;
layout(location = 1) in vec2 pos_center;
layout(location = 2) in vec2 tex_coord_center;

layout(location = 0) out float alpha;
layout(location = 1) out vec2 tex_coord;

void main() {
  gl_Position =
      vec4(pos_center + vertices_info.pos_tex_coords[gl_VertexIndex].xy,
           0.0, 1.0);
  alpha = in_alpha;
  tex_coord = tex_coord_center +
              vertices_info.pos_tex_coords[gl_VertexIndex].zw;
}
