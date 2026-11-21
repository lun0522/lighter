//
//  air_transmit_table.cc
//
//  Created by Pujun Lun on 4/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/aurora/viewer/air_transmit_table.h"

#include <glm/glm.hpp>
#include <limits>
#include <vector>

#include "lighter/base/util.h"

namespace lighter::example::aurora {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEarthRadius = 6378.1f;
constexpr float kAirMaxHeight = 75.0f;

struct Ray {
  glm::vec3 start;
  glm::vec3 direction;
};

struct SpanT {
  float low;
  float high;
};

struct Sphere {
  glm::vec3 center;
  float radius;
};

SpanT GetSpanSphere(const Ray& ray, const Sphere& sphere) {
  const glm::vec3 sphere_to_ray = ray.start - sphere.center;
  const float b = 2.0f * glm::dot(sphere_to_ray, ray.direction);
  const float c =
      glm::dot(sphere_to_ray, sphere_to_ray) - sphere.radius * sphere.radius;
  const float det = b * b - 4.0f * c;
  ASSERT_TRUE(det > 0.0f, "Ray must start inside atmosphere sphere");
  const float sd = glm::sqrt(det);
  return {/*low=*/(-b - sd) * 0.5f, /*high=*/(-b + sd) * 0.5f};
}

float GetErfGuts(float x) {
  constexpr float a = 8.0f * (kPi - 3.0f) / (3.0f * kPi * (4.0f - kPi));
  const float x_sqr = x * x;
  return static_cast<float>(
      glm::exp(-x_sqr * (4.0f / kPi + a * x_sqr) / (1.0f + a * x_sqr)));
}

float GetWinErf(float x) {
  const float sign = x < 0.0f ? -1.0f : 1.0f;
  return sign * glm::sqrt(1.0f - GetErfGuts(x));
}

float GetWinErfc(float x) {
  return x > 3.0f ? 0.5f * GetErfGuts(x) : 1.0f - GetWinErf(x);
}

float GetAtmosphereThickness(const Ray& ray, const SpanT& span) {
  constexpr float scale_height = 8.0f / kEarthRadius;
  constexpr float k = 1.0f / scale_height;
  constexpr float ref_ht = 1.0f;
  constexpr float ref_den = 100.0f;
  const float norm = static_cast<float>(glm::sqrt(kPi)) / 2.0f;

  const float a = glm::dot(ray.direction, ray.direction);
  const float b = 2.0f * glm::dot(ray.direction, ray.start);
  const float c = glm::dot(ray.start, ray.start);
  const float tc = -b / (2.0f * a);
  const float y = glm::sqrt(tc * tc * a + tc * b + c);
  float xl = span.low - tc;
  float xr = span.high - tc;

  const float y_sqr = y * y;
  const float xl_sqr = xl * xl;
  const float xr_sqr = xr * xr;
  float r1_sqr = 0.0f;
  float r1 = 0.0f;
  bool is_cross = false;
  if (xl * xr < 0.0f) {
    r1_sqr = y_sqr;
    r1 = y;
    is_cross = true;
  } else {
    r1_sqr = xl_sqr + y_sqr;
    if (r1_sqr > xr_sqr + y_sqr) {
      r1_sqr = xr_sqr + y_sqr;
    }
    r1 = glm::sqrt(r1_sqr);
  }

  const float del = 2.0f / k;
  const float r2 = r1 + del;
  const float r2_sqr = r2 * r2;

  const float x1_sqr = r1_sqr - y_sqr;
  const float x2_sqr = r2_sqr - y_sqr;

  const float C = (r1 - r2) / (x1_sqr - x2_sqr);
  const float A = r1 - x1_sqr * C - ref_ht;

  const float sqrt_kC = glm::sqrt(k * C);
  float erf_del = 0.0f;
  if (is_cross) {
    erf_del = GetWinErf(sqrt_kC * xr) - GetWinErf(sqrt_kC * xl);
  } else {
    if (xl < 0.0f) {
      xl = -xl;
      xr = -xr;
    }
    erf_del = GetWinErfc(sqrt_kC * xr) - GetWinErfc(sqrt_kC * xl);
  }
  if (glm::abs(erf_del) > 1E-10) {
    const float e_scl = glm::exp(-k * A);
    return ref_den * norm * e_scl / sqrt_kC * glm::abs(erf_del);
  }
  const float x1 = glm::sqrt(x1_sqr);
  const float x2 = glm::sqrt(x2_sqr);
  const float M = (r2 - r1) / (x2 - x1);
  const float B = r1 - M * x1 - 1.0f;
  const float t1 = glm::exp(-k * (M * xl + B));
  const float t2 = glm::exp(-k * (M * xr + B));
  return glm::abs(ref_den * (t2 - t1) / (k * M));
}

}  // namespace

base::Image GenerateAirTransmitTable(float sample_step) {
  constexpr int kImageWidth = 1;
  const int image_height = static_cast<int>(glm::floor(1.0f / sample_step));
  std::vector<unsigned char> image_data(static_cast<size_t>(image_height));

  for (int i = 0; i < image_height; ++i) {
    const float angle = glm::acos(sample_step * static_cast<float>(i));
    const Ray ray{/*start=*/{0.0f, 0.0f, 1.0f},
                  /*direction=*/{glm::sin(angle), 0.0f, glm::cos(angle)}};
    const Sphere air_layer{/*center=*/glm::vec3{0.0f},
                           /*radius=*/kAirMaxHeight / kEarthRadius + 1.0f};
    const SpanT air_span = GetSpanSphere(ray, air_layer);
    const float air_mass =
        GetAtmosphereThickness(ray, SpanT{/*low=*/0.0f, air_span.high});
    const float air_transmit =
        glm::exp(-air_mass) * std::numeric_limits<unsigned char>::max();
    image_data[static_cast<size_t>(i)] =
        static_cast<unsigned char>(glm::round(air_transmit));
  }

  const base::Image::Dimension dimension{kImageWidth, image_height,
                                         base::image::kBwImageChannel};
  return base::Image::LoadSingleImageFromMemory(dimension, image_data.data(),
                                                /*flip_y=*/false);
}

}  // namespace lighter::example::aurora
