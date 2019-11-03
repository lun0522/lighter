#version 460 core

#if defined(TARGET_OPENGL)
layout(binding = 1) uniform TextureIndex {
  int value;
} texture_index;

#elif defined(TARGET_VULKAN)
layout(push_constant) uniform TextureIndex {
  int value;
} texture_index;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(binding = 2) uniform sampler2D tex_sampler[2];

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = texture(tex_sampler[texture_index.value], tex_coord);
}
