#version 460 core

layout(location = 0) in vec4 color_alpha;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = color_alpha;
}
