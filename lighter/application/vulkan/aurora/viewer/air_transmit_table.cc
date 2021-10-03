//
//  air_transmit_table.cc
//
//  Created by Pujun Lun on 4/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/aurora/viewer/air_transmit_table.h"

#include <limits>

#include "lighter/common/util.h"

#define M_PI 3.14159265358979323846

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

constexpr float kEarthRadius = 6378.1f;
constexpr float kAirMaxHeight = 75.0f;

// A 3D ray shooting through space.
struct Ray {
  glm::vec3 start;
  glm::vec3 direction;
};

// A span of ray t values.
struct SpanT {
  float low;
  float high;
};

// A 3D earth model.
struct Sphere {
  glm::vec3 center;
  float radius;
};

// Returns the span of t values at intersection region of 'ray' and 'sphere'.
SpanT GetSpanSphere(const Ray& ray, const Sphere& sphere) {
  const glm::vec3 sphere_to_ray = ray.start - sphere.center;
  const float b = 2.0f * glm::dot(sphere_to_ray, ray.direction);
  const float c = glm::dot(sphere_to_ray, sphere_to_ray) -
                  sphere.radius * sphere.radius;
  const float det = b * b - 4.0f * c;
  ASSERT_TRUE(det > 0.0f, "we shoot a ray from inside of the sphere");
  const float sd = glm::sqrt(det);
  return {/*low=*/(-b - sd) * 0.5f, /*high=*/(-b + sd) * 0.5f};
}

/* BEGIN: Atmosphere integral approximation. */

// Decent little Wikipedia/Winitzki 2003 approximation to erf.
// Supposedly accurate to within 0.035% relative error.
float GetErfGuts(float x) {
  constexpr float a = 8.0f * (M_PI - 3.0f) / (3.0f * M_PI * (4.0f - M_PI));
  const float x_sqr = x * x;
  return static_cast<float>(
      glm::exp(-x_sqr * (4.0f / M_PI + a * x_sqr) / (1.0f + a * x_sqr)));
}

// "Error function": integral of exp(-x*x).
float GetWinErf(float x) {
  const float sign = x < 0.0f ? -1.0f : 1.0f;
  return sign * glm::sqrt(1.0f - GetErfGuts(x));
}

// erfc = 1.0-erf, but with less round-off.
float GetWinErfc(float x) {
  // If x is big, erf(x) is very close to +1.0.
  // erfc(x)=1-erf(x)=1-sqrt(1-e)=approx +e/2
  return x > 3.0f ? 0.5f * GetErfGuts(x) : 1.0f - GetWinErf(x);
}

// Computes the atmosphere's integrated thickness along 'ray' within 'span'.
// The planet is assumed to be centered at origin, with unit radius.
// This is an exponential approximation.
float GetAtmosphereThickness(const Ray& ray, const SpanT& span) {
  // Where atmosphere reaches 1/e thickness (planetary radius units).
  constexpr float scale_height = 8.0f / kEarthRadius;
  // Atmosphere density = ref_den*exp(-(height-ref_ht)*k).
  constexpr float k = 1.0f / scale_height;
  // Height where density==ref_den.
  constexpr float ref_ht = 1.0f;
  // Atmosphere opacity per planetary radius.
  constexpr float ref_den = 100.0f;
  // Normalization constant.
  const float norm = static_cast<float>(glm::sqrt(M_PI)) / 2.0f;

  // Step 1: planarize problem from 3D to 2D. Integral is along 'ray'.
  const float a = glm::dot(ray.direction, ray.direction);
  const float b = 2.0f * glm::dot(ray.direction, ray.start);
  const float c = glm::dot(ray.start, ray.start);
  const float tc = -b / (2.0f * a);  // t value at ray/origin closest approach.
  const float y = glm::sqrt(tc * tc * a + tc * b + c);
  float xl = span.low - tc;
  float xr = span.high - tc;
  // Integral is along line: from xl to xr at given y.
  // x==0 is the point of closest approach.

  // Step 2: Find first matching radius r1 -- smallest used radius.
  const float y_sqr = y * y;
  const float xl_sqr = xl * xl;
  const float xr_sqr = xr * xr;
  float r1_sqr, r1;
  bool is_cross = false;
  if (xl * xr < 0.0) {
    // Span crosses origin -- use radius of closest approach.
    r1_sqr = y_sqr;
    r1 = y;
    is_cross = true;
  } else {
    // Use either left or right endpoint -- whichever is closer to surface.
    r1_sqr = xl_sqr + y_sqr;
    if (r1_sqr > xr_sqr + y_sqr) {
      r1_sqr = xr_sqr + y_sqr;
    }
    r1 = glm::sqrt(r1_sqr);
  }

  // Step 3: Find second matching radius r2.
  const float del = 2.0f / k;  // 80% of atmosphere (at any height).
  const float r2 = r1 + del;
  const float r2_sqr = r2 * r2;

  // Step 4: Find parameters for parabolic approximation to true hyperbolic
  // distance.
  // r(x)=sqrt(y^2+x^2), r'(x)=A+Cx^2; r1=r1', r2=r2'
  // r_sqr = x_sqr + y_sqr, so x_sqr = r_sqr - y_sqr
  const float x1_sqr = r1_sqr - y_sqr;
  const float x2_sqr = r2_sqr - y_sqr;

  const float C = (r1 - r2) / (x1_sqr - x2_sqr);
  const float A = r1 - x1_sqr * C - ref_ht;

  // Step 5: Compute the integral of exp(-k*(A+Cx^2)) from x==xl to x==xr.
  const float sqrt_kC = sqrt(k * C);  // Variable change: z=sqrt(k*C)*x;
                                      //                  exp(-z^2)
  float erf_del;
  if (is_cross) {
    // xl and xr have opposite signs -- use erf normally.
    erf_del = GetWinErf(sqrt_kC * xr) - GetWinErf(sqrt_kC * xl);
  } else {
    //xl and xr have same sign -- flip to positive half and use erfc.
    if (xl < 0.0f) {
      xl = -xl;
      xr = -xr;
    }
    erf_del = GetWinErfc(sqrt_kC * xr) - GetWinErfc(sqrt_kC * xl);
  }
  if (glm::abs(erf_del) > 1E-10) {
    // Parabolic approximation has acceptable round-off.
    const float e_scl = glm::exp(-k * A);  // From constant term of integral.
    return ref_den * norm * e_scl / sqrt_kC * glm::abs(erf_del);
  } else {
    // erf_del==0.0 -> Round-off!
    // Switch to a linear approximation:
    //   a.) Create linear approximation r(x) = M*x+B
    //   b.) Integrate exp(-k*(M*x+B-1.0)) dx, from xl to xr
    //   integral = (1.0/(-k*M)) * exp(-k*(M*x+B-1.0))
    const float x1 = glm::sqrt(x1_sqr);
    const float x2 = glm::sqrt(x2_sqr);
    // Linear fit at (x1,r1) and (x2,r2).
    const float M = (r2 - r1) / (x2 - x1);
    const float B = r1 - M * x1 - 1.0f;

    const float t1 = glm::exp(-k * (M * xl + B));
    const float t2 = glm::exp(-k * (M * xr + B));
    return glm::abs(ref_den * (t2 - t1) / (k * M));
  }
}

/* END: Atmosphere integral approximation. */

} /* namespace */

common::Image GenerateAirTransmitTable(float sample_step) {
  constexpr int kImageWidth = 1;
  const int image_height = glm::floor(1.0f / sample_step);
  auto* image_data = new unsigned char[kImageWidth * image_height];

  for (int i = 0; i < image_height; ++i) {
    const float angle = glm::acos(sample_step * static_cast<float>(i));
    const Ray ray{/*start=*/{0.0f, 0.0f, 1.0f},
                  /*direction=*/{glm::sin(angle), 0.0f, glm::cos(angle)}};
    const Sphere air_layer{/*center=*/glm::vec3{0.0f},
                           /*radius=*/kAirMaxHeight / kEarthRadius + 1.0f};
    const SpanT air_span = GetSpanSphere(ray, air_layer);
    const float air_mass =
        GetAtmosphereThickness(ray, SpanT{/*low=*/0.0f, air_span.high});
    const float air_transmit = glm::exp(-air_mass) *
                               std::numeric_limits<unsigned char>::max();
    image_data[i] = glm::round(air_transmit);
  }

  const common::Image::Dimension dimension{kImageWidth, image_height,
                                           common::image::kBwImageChannel};
  auto table = common::Image::LoadSingleImageFromMemory(dimension, image_data,
                                                        /*flip_y=*/false);
  delete[] image_data;
  return table;
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
