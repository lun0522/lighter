#version 460 core

layout(std140, push_constant) uniform Alpha {
  float value;
} alpha;

layout(location = 0) in vec3 color;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = vec4(color, alpha.value);
}
