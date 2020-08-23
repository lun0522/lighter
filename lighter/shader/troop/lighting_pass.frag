#version 460 core

#define NUM_LIGHTS 32

layout(std140, binding = 0) uniform Lights {
  vec4 colors[NUM_LIGHTS];
} lights;

layout(std140, binding = 1) uniform RenderInfo {
  vec4 light_centers[NUM_LIGHTS];
  vec4 camera_pos;
} render_info;

layout(binding = 2) uniform sampler2D pos_sampler;
layout(binding = 3) uniform sampler2D norm_sampler;
layout(binding = 4) uniform sampler2D diff_spec_sampler;

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 frag_color;

const float linear = 0.7;
const float quadratic = 1.8;

void main() {
  const vec3 frag_pos = texture(pos_sampler, tex_coord).rgb;
  const vec3 norm = texture(norm_sampler, tex_coord).rgb;
  const vec4 diff_spec = texture(diff_spec_sampler, tex_coord);
  const vec3 view_dir = normalize(render_info.camera_pos.xyz - frag_pos);

  vec3 color = diff_spec.rgb * 0.1;
  for(int i = 0; i < NUM_LIGHTS; ++i) {
    // Diffuse.
    const vec3 light_center = render_info.light_centers[i].xyz;
    const vec3 light_dir = normalize(light_center - frag_pos);
    const float diff = max(dot(norm, light_dir), 0.0);
    const vec3 diffuse = lights.colors[i].rgb * diff * diff_spec.rgb;

    // Specular.
    const vec3 half_dir = normalize(light_dir + view_dir);
    const float spec = pow(max(dot(norm, half_dir), 0.0), 16.0);
    const vec3 specular = lights.colors[i].rgb * spec * diff_spec.a;

    // Attenuation.
    const float distance = length(light_center - frag_pos);
    const float attenuation =
        1.0 / (1.0 + linear * distance + quadratic * distance * distance);
    color += (diffuse + specular) * attenuation;
  }
  frag_color = vec4(color, 1.0);
}
