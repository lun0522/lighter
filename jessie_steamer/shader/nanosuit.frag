#version 460 core

#if defined(TARGET_OPENGL)
layout(binding = 1) uniform TransFrag {
  mat4 view_inv;
} trans_frag;

#elif defined(TARGET_VULKAN)
layout(push_constant) uniform TransFrag {
  mat4 view_inv;
} trans_frag;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(binding = 1) uniform sampler2D diff_sampler;
layout(binding = 2) uniform sampler2D spec_sampler;
layout(binding = 3) uniform sampler2D refl_sampler;
layout(binding = 4) uniform samplerCube skybox_sampler;

layout(location = 0) in vec3 pos_view;
layout(location = 1) in vec3 norm_view;
layout(location = 2) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
  vec3 color = texture(diff_sampler, tex_coord).rgb +
               texture(spec_sampler, tex_coord).rgb * 0.3f;
  vec3 view_dir = normalize(-pos_view);  // view space
  vec3 refl_dir = reflect(-view_dir, normalize(norm_view));
  refl_dir = (trans_frag.view_inv * vec4(refl_dir, 0.0)).xyz;  // world space
  vec3 env_color = texture(skybox_sampler, refl_dir).rgb;
  color = mix(color, env_color, texture(refl_sampler, tex_coord).r);
  frag_color = vec4(color, 1.0);
}
