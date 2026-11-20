#version 460 core

layout(std140, push_constant) uniform Transformation {
  mat4 proj_view;
} trans;

layout(location = 0) in vec3 in_pos;

void main() {
  gl_Position = trans.proj_view * vec4(in_pos, 1.0);
}
