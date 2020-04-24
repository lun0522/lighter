/*
 This shader is modified from Dr. Orion Sky Lawlor's implementation.
 Lawlor, Orion & Genetti, Jon. (2011). Interactive Volume Rendering Aurora on
 the GPU. Journal of WSCG. 19. 25-32.

 Original header:
 GLSL fragment shader: raytracer with planetary atmosphere.
 Dr. Orion Sky Lawlor, olawlor@acm.org, 2010-09-04 (Public Domain)
 */

#version 460 core

#define M_PI 3.1415926535897932384626433832795

layout(location = 0) in vec3 frag_dir;

layout(location = 0) out vec4 frag_color;

layout(binding = 0) uniform RenderInfo {
  vec4 camera_pos;
  mat4 aurora_proj_view;
} render_info;

// Y coordinate corresponds to the height from 0km (ground) to 300km (greatest
// height of aurora layer).
layout(binding = 1) uniform sampler2D aurora_deposition_sampler;
layout(binding = 2) uniform sampler2D aurora_paths_sampler;
// Higher value means greater distance to the closest point on aurora paths.
layout(binding = 3) uniform sampler2D distance_field_sampler;
// Y coordinate corresponds to the cosine value of angle between camera up
// vector and the direction of view ray.
layout(binding = 4) uniform sampler2D air_transmit_sampler;
layout(binding = 5) uniform samplerCube universe_skybox_sampler;

// Earth radius is used as one render unit.
const float earth_radius = 6378.1;
const float km = 1.0 / earth_radius;
const float aurora_min_height = 85.0 * km;
const float aurora_max_height = 300.0 * km;
// Sampling rate for aurora. Fine sampling takes longer time.
const float dt = 2.0 * km;
// Scaling factor for sampled aurora color.
const float aurora_scale = dt / (40.0 * km);
// t value for a miss ray.
const float miss_t = 100.0;
const vec3 air_color = 0.002 * vec3(0.4, 0.5, 0.7);

// A 3D ray shooting through space.
struct Ray {
  vec3 start, direction;
};

// A span of ray t values.
struct SpanT {
  float low, high;
};

// A sphere model.
struct Sphere {
  vec3 center;
  float radius;
};

// Returns a span of t values at intersection region of 'ray' and 'sphere'.
SpanT GetSpanSphere(Ray ray, Sphere sphere) {
  const vec3 sphere_to_ray = ray.start - sphere.center;
  const float b = 2.0 * dot(sphere_to_ray, ray.direction);
  const float c = dot(sphere_to_ray, sphere_to_ray) -
                  sphere.radius * sphere.radius;
  const float det = b * b - 4.0 * c;
  if (det < 0.0) {
    // Ray totally miss.
    return SpanT(miss_t, miss_t);
  }
  const float sd = sqrt(det);
  return SpanT((-b - sd) * 0.5, (-b + sd) * 0.5);
}

// Converts the 3D 'point' to 2D texture coordinates. This can be used for
// sampling the aurora paths texture and distance field texture.
vec2 ProjectToPlane(vec3 point) {
  const vec4 projected = render_info.aurora_proj_view * vec4(point, 1.0);
  // Perform perspective division, and map values from range [-1, 1] to [0, 1].
  return (projected.xy / projected.w + 1.0) / 2.0;
}

// Returns the color of aurora at 'sample_point'.
vec3 SampleAurora(vec3 sample_point) {
  const float paths_intensity = texture(aurora_paths_sampler,
                                        ProjectToPlane(sample_point)).r;
  // Subtract off radius of planet since deposition texture starts from ground.
  const float height = length(sample_point) - 1.0;
  const vec3 deposition = texture(aurora_deposition_sampler,
                                  vec2(0.4, height / aurora_max_height)).rgb;
  return deposition * paths_intensity;
}

// Samples the color of aurora along 'ray', and returns the summed color.
vec3 SampleAurora(Ray ray, SpanT span) {
  // Start sampling from above the ground, and sum up aurora lights along the
  // ray within span.
  float t = max(span.low, 0.0);
  vec3 sum = vec3(0.0);
  while (t < span.high) {
    const vec3 sample_point = ray.start + ray.direction * t;
    sum += SampleAurora(sample_point);
    t += dt;
    // TODO: Distance field is not used.
//    const float dist = 0.2 * texture(distance_field_sampler,
//                                     ProjectToPlane(sample_point)).r;
//    t += max(dist, dt);
  }
  return sum * aurora_scale;
}

void main() {
  // Compute intersection with planet itself.
  const vec3 normal = normalize(vec3(render_info.camera_pos));
  vec3 normalized_dir = normalize(frag_dir);
  const float cos_angle = dot(normalized_dir, normal);
  // If the view ray hits ground, create the reflection effect.
  if (cos_angle < 0) {
    normalized_dir -= 2 * cos_angle * normal;
  }

  // Sample the view ray when it travels from 'aurora_min_height' to
  // 'aurora_max_height'.
  const Ray ray = Ray(vec3(render_info.camera_pos), normalized_dir);
  const float aurora_min_t =
      GetSpanSphere(ray, Sphere(vec3(0.0), 1.0 + aurora_min_height)).high;
  const float aurora_max_t =
      GetSpanSphere(ray, Sphere(vec3(0.0), 1.0 + aurora_max_height)).high;
  const vec3 aurora_color =
      SampleAurora(ray, SpanT(aurora_min_t, aurora_max_t));

  // We need to take atmosphere (i.e. thinkness of air) into consideration.
  const float air_transmit = texture(air_transmit_sampler,
                                     vec2(0.5, cos_angle)).r;
  const vec3 foreground = air_transmit * aurora_color +
                          (1.0 - air_transmit) * air_color;
  const vec3 background = texture(universe_skybox_sampler, normalized_dir).rgb;
  // No need to convert to sRGB since hardware will perform it automatically.
  frag_color = vec4(foreground + (1.0 - length(foreground)) * background, 1.0);

  // Assume reflectance is 0.5.
  if (cos_angle < 0.0) {
    frag_color.rgb *= 0.5;
  }
}
