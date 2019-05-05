#version 460 core

#ifdef TARGET_OPENGL
layout(binding = 0) uniform Transformation {
  mat4 view_model;
  mat4 proj_view_model;
  mat4 view_model_inv_trs;
  mat4 view_inv;
} trans;
#endif

#ifdef TARGET_VULKAN
layout(push_constant) uniform Transformation {
  mat4 view_model;
  mat4 proj_view_model;
  mat4 view_model_inv_trs;
  mat4 view_inv;
} trans;
#endif

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
  refl_dir = (trans.view_inv * vec4(refl_dir, 0.0)).xyz;  // back to world space
  vec3 env_color = texture(skybox_sampler, refl_dir).rgb;
  color = mix(color, env_color, texture(refl_sampler, tex_coord).r);
  frag_color = vec4(color, 1.0);
}
