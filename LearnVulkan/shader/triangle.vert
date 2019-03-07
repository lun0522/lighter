#version 450
#extension GL_ARB_separate_shader_objects : enable

// possible to use layout(set = 0, binding = 0) to bind multiple descriptor sets
// to one binding point, which can be useful if we render different objects with
// different buffers and descriptors, but use the same uniform values
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
