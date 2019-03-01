#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

layout (location = 0) out vec3 color;

void main() {
  gl_Position = vec4(aPos, 0.0, 1.0);
  color = aColor;
}
