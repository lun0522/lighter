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

layout(binding = 0) uniform Camera {
  mat4 aurora_proj_view;
  vec4 pos;
  vec4 up;
  vec4 front;
  vec4 right;
} camera;

layout(binding = 1) uniform sampler2D auroraDeposition; // deposition function
layout(binding = 2) uniform sampler2D auroraTexture; // actual curtains and color
layout(binding = 3) uniform sampler2D distanceField; // distance from curtains (1==far away, 0==close)
layout(binding = 4) uniform sampler2D airTransTable;
//uniform samplerCube skybox;

const float km = 1.0 / 6378.1; // convert kilometers to render units (planet radii)
const float miss_t = 100.0; // t value for a miss
const float dt = 2.0 * km; // sampling rate for aurora: fine sampling gets *SLOW*
const float auroraScale = dt / (40.0 * km); // scale factor: samples at dt -> screen color
const float airSampleStep = 0.01;
const vec3 airColor = 0.002 * vec3(0.4, 0.5, 0.7);

/* A 3D ray shooting through space */
struct ray {
  vec3 S, D; /* start location and direction (unit length) */
};
vec3 ray_at(ray r, float t) { return r.S + r.D * t; }

/* A span of ray t values */
struct span {
  float l, h; /* lowest, and highest t value */
};

struct sphere {
  vec3 center;
  float r;
};

/* Return span of t values at intersection region of ray and sphere */
span span_sphere(sphere s, ray r) {
  float b = 2.0 * dot(r.S - s.center, r.D);
  float c = dot(r.S - s.center, r.S - s.center) - s.r * s.r;
  float det = b * b - 4.0 * c;
  if (det < 0.0) return span(miss_t, miss_t); /* total miss */
  float sd = sqrt(det);
  float tL = (-b - sd) * 0.5;
  float tH = (-b + sd) * 0.5;
  return span(tL, tH);
}

/* Return the amount of auroral energy deposited at this height,
 measured in planetary radii. */
vec3 deposition_function(float height) {
  height -= 1.0; /* convert to altitude (subtract off planet's radius) */
  float maxHeight = 300.0 * km;
  return vec3(texture(auroraDeposition, vec2(0.8, height / maxHeight)));
}

/* Return air transmit value at this angle (should pass in cos value of the angle) */
float air_transmit(float cosVal) {
  int numStep = int(round(1.0 / airSampleStep)) + 1;
  float stepSize = 1.0 / numStep;
  float xOffset = (1.0 - stepSize) * cosVal + stepSize / 2.0;
  return texture(airTransTable, vec2(xOffset, 0.5)).r * 255.0 / 200.0;
}

// Apply nonlinear tone mapping to final summed output color
vec3 tone_map(vec3 color) {
  float len = length(color);
  return color * pow(len, 1.0 / 2.2 - 1.0); /* convert to sRGB: scale by len^(1/2.2)/len */
}

/* Convert a 3D location to a 2D aurora map index (polar stereographic) */
vec2 down_to_map(vec3 pos_world) {
  const vec4 projected = camera.aurora_proj_view * vec4(pos_world, 1.0);
  // Perform perspective division, and map values from range [-1, 1] to [0, 1].
  return (projected.xy / projected.w + 1.0) / 2.0;
}

/* Sample the aurora's color at this 3D point */
vec3 sample_aurora(vec3 loc) {
  /* project sample point to surface of planet, and look up in texture */
  float r = length(loc);
  vec3 deposition = deposition_function(r);
  vec3 curtain = vec3(texture(auroraTexture, down_to_map(loc)).r);
  return deposition * curtain;
}

/* Sample the aurora's color along this ray, and return the summed color */
vec3 sample_aurora(ray r, span s) {
  if (s.h < 0.0) return vec3(0.0); /* whole span is behind our head */
  if (s.l < 0.0) s.l = 0.0; /* start sampling at observer's head */

  /* Sum up aurora light along ray span */
  vec3 sum = vec3(0.0);
  float t = s.l;
  while (t < s.h) {
    vec3 loc = ray_at(r, t);
    sum += sample_aurora(loc); // real curtains
    // TODO: Distance field is not used.
//    float dist = texture(distanceField, down_to_map(loc)).r * 0.2;
//    if (dist < dt) dist = dt;
    t += dt;
  }

  return sum * auroraScale; // full curtain
}

void main() {
  // Compute intersection with planet itself
  const vec3 normal = normalize(vec3(camera.pos));
  vec3 normalized_dir = normalize(frag_dir);
  bool is_gound = false;
  const float projection = dot(normalized_dir, normal);
  if (projection < 0) {
    // Create reflection effect.
    is_gound = true;
    normalized_dir += -2 * projection * normal;
  }

  // Start with a camera ray
  ray r = ray(vec3(camera.pos), normalized_dir);

  // All our geometry
  span auroraL = span_sphere(sphere(vec3(0.0), 85.0 * km + 1.0), r);
  span auroraH = span_sphere(sphere(vec3(0.0), 300.0 * km + 1.0), r);

  // Atmosphere
  float airTransmit = air_transmit(projection);
  float airInscatter = 1.0 - airTransmit; // fraction added by atmosphere

  // typical planet-hitting ray
  vec3 aurora = sample_aurora(r, span(auroraL.h, auroraH.h));

  vec3 total = airTransmit * aurora + airInscatter * airColor;

  // Must delay tone mapping until the very end, so we can sum pre and post atmosphere parts...
  vec3 foreground = tone_map(total);
  //  vec3 background = vec3(texture(skybox, cameraDir));
  vec3 background = vec3(0.0);
  float bgStrength = 1.0 - length(foreground);
  frag_color = vec4(foreground + bgStrength * background, 1.0);

  // assume reflectance 0.5
  if (is_gound) {
    frag_color *= 0.5;
  }
}
