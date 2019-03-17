#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (binding = 0) uniform UniformBufferObject {
  mat4 model;
  mat4 view;
  mat4 proj;
} ubo;

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec3 in_color;

layout (location = 0) out vec3 color;

void main() {
  gl_Position = ubo.proj * ubo.view * ubo.model * vec4(in_pos, 0.0, 1.0);
  color = in_color;
}
