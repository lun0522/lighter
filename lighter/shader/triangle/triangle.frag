#version 460 core

#if defined(TARGET_OPENGL)
// TODO: Add binding = 0 back.
layout(std140) uniform Alpha {
  float value;
} alpha;

#elif defined(TARGET_VULKAN)
layout(std140, push_constant) uniform Alpha {
  float value;
} alpha;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(location = 0) in vec3 color;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = vec4(color, alpha.value);
}
