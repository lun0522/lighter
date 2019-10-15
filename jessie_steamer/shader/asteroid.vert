#version 460 core

#if defined(TARGET_OPENGL)
layout(binding = 0) uniform Transformation {
  mat4 model;
  mat4 proj_view;
} trans;

#elif defined(TARGET_VULKAN)
layout(push_constant) uniform Transformation {
  mat4 model;
  mat4 proj_view;
} trans;

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(binding = 1) uniform Light {
  vec4 direction_time;
} light;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_norm;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in float theta;
layout(location = 4) in float radius;
layout(location = 5) in mat4 model;  // Takes up 4 binding locations.

layout(location = 0) out vec3 norm;
layout(location = 1) out vec2 tex_coord;

void main() {
  float extra_angle = light.direction_time.w * 0.1;
  float sin_ea = sin(extra_angle), cos_ea = cos(extra_angle);
  float angle = theta + extra_angle;
  vec3 center = vec3(sin(angle) * radius, 0.0, cos(angle) * radius);
  mat4 to_world = mat4(vec4(cos_ea, 0.0, -sin_ea, 0.0),
                       vec4(   0.0, 1.0,     0.0, 0.0),
                       vec4(sin_ea, 0.0,  cos_ea, 0.0),
                       vec4(              center, 1.0));
  vec4 pos_world = to_world * model * vec4(in_pos, 1.0);
  // Since the orientation of each asteroid changes per-frame, passing inversed
  // transposed model matrix to calculate normals in world space would be too
  // expensive. We approximate the normal by pretending an asteroid as a sphere.
  norm = normalize(pos_world.xyz / pos_world.w - center);
  gl_Position = trans.proj_view * pos_world;
  tex_coord = in_tex_coord;
}
