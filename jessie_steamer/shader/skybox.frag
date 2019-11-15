#version 460 core

layout(binding = 1) uniform samplerCube skybox_sampler;
layout(binding = 2) uniform sampler2D button_sampler;  // TODO: Remove this.

layout(location = 0) in vec3 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = texture(button_sampler, vec2(0.0f, 0.0f));  // TODO: Remove this.
  frag_color = texture(skybox_sampler, tex_coord);
}
