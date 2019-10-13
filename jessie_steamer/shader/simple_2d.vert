#version 460 core

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 color;

void main() {
  gl_Position = vec4(in_pos, 1.0);
  color = in_color;
}
