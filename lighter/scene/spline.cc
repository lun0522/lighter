//
//  spline.cc
//
//  Created by Pujun Lun on 11/30/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/scene/spline.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <glm/gtx/vector_angle.hpp>

#include "lighter/base/util.h"

namespace lighter::scene {
namespace {

constexpr float kEpsilon = 1E-6f;
constexpr float kMinSegmentDist = 1E-5f;

// Returns the angle between 'a' and 'b' in radians, returning 0 if either
// vector is degenerate so collapsed Bezier handles or coincident points never
// yield NaN or trigger unbounded subdivision.
float SafeAngleBetween(const glm::vec3& a, const glm::vec3& b) {
  const float len_a_sq = glm::dot(a, a);
  const float len_b_sq = glm::dot(b, b);
  if (len_a_sq <= kEpsilon * kEpsilon || len_b_sq <= kEpsilon * kEpsilon) {
    return 0.0f;
  }
  return glm::angle(a / std::sqrt(len_a_sq), b / std::sqrt(len_b_sq));
}

}  // namespace

BezierSpline::BezierSpline(int max_recursion_depth, IsSmooth&& is_smooth,
                           ProjectPoint&& project_point)
    : max_recursion_depth_{max_recursion_depth},
      is_smooth_{std::move(FATAL_IF_NULL(is_smooth))},
      project_point_{std::move(project_point)} {}

void BezierSpline::Tessellate(const glm::vec3& p0, const glm::vec3& p1,
                              const glm::vec3& p2, const glm::vec3& p3,
                              int recursion_depth) {
  // Check endpoints and outer handle legs together so a curved sub-segment with
  // close endpoints but large handles still subdivides, while sub-threshold
  // control polygons terminate safely.
  constexpr float kMinDistBetweenPoints = 1E-4f;
  const bool tiny_polygon = glm::distance(p0, p3) < kMinDistBetweenPoints &&
                            glm::distance(p0, p1) < kMinDistBetweenPoints &&
                            glm::distance(p2, p3) < kMinDistBetweenPoints;
  if (++recursion_depth >= max_recursion_depth_ || tiny_polygon ||
      is_smooth_(p0, p1, p2, p3)) {
    // Only emit 'p0' at each leaf: left-to-right recursion visits segments in
    // order, and the caller appends the closing point. Apply 'project_point_'
    // here rather than during subdivision to preserve De Casteljau's affine
    // weights.
    mutable_splines()->push_back(project_point_ ? project_point_(p0) : p0);
    return;
  }

  // Standard De Casteljau midpoint split at u = 0.5 into left [p0, p10, p20,
  // p30] and right [p30, p21, p12, p3] cubic Bezier sub-segments.
  const glm::vec3 p10 = 0.5f * (p0 + p1);
  const glm::vec3 p11 = 0.5f * (p1 + p2);
  const glm::vec3 p12 = 0.5f * (p2 + p3);
  const glm::vec3 p20 = 0.5f * (p10 + p11);
  const glm::vec3 p21 = 0.5f * (p11 + p12);
  const glm::vec3 p30 = 0.5f * (p20 + p21);
  Tessellate(p0, p10, p20, p30, recursion_depth);
  Tessellate(p30, p21, p12, p3, recursion_depth);
}

const int CatmullRomSpline::kMinNumControlPoints = 3;

std::unique_ptr<Spline> CatmullRomSpline::GetOnSphereSpline(
    int max_recursion_depth, float roughness) {
  BezierSpline::ProjectPoint project_point = [](const glm::vec3& p) {
    return glm::dot(p, p) > kEpsilon * kEpsilon ? glm::normalize(p) : p;
  };

  BezierSpline::IsSmooth is_smooth =
      [roughness, project_point](const glm::vec3& p0, const glm::vec3& p1,
                                 const glm::vec3& p2, const glm::vec3& p3) {
        const glm::vec3 e0 = p1 - p0;
        const glm::vec3 e1 = p2 - p1;
        const glm::vec3 e2 = p3 - p2;
        // Check adjacent and end-to-end control-polygon turns in Euclidean
        // space, plus the angular step after spherical projection: chords
        // passing deep inside the sphere appear flat in R^3 yet stretch in
        // angle when projected onto the unit sphere.
        return SafeAngleBetween(e0, e1) <= roughness &&
               SafeAngleBetween(e1, e2) <= roughness &&
               SafeAngleBetween(e0, e2) <= roughness &&
               SafeAngleBetween(project_point(p0), project_point(p3)) <=
                   4.0f * roughness;
      };

  return std::make_unique<CatmullRomSpline>(
      max_recursion_depth, std::move(is_smooth), std::move(project_point));
}

void CatmullRomSpline::Tessellate(const glm::vec3& p0, const glm::vec3& p1,
                                  const glm::vec3& p2, const glm::vec3& p3) {
  // Degenerate zero-length segment (adjacent coincident anchors): emit the
  // anchor once and return without dividing by zero.
  if (glm::distance(p1, p2) <= kMinSegmentDist) {
    BezierSpline::Tessellate(p1, p1, p1, p1, /*recursion_depth=*/0);
    return;
  }

  // Centripetal Catmull-Rom sets knot spacing dt_i = ||P_{i+1} - P_i||^0.5
  // (alpha = 0.5). Compared to uniform spacing (dt_i = 1), this scales tangent
  // handles with segment length so closely spaced anchors next to long spans
  // do not overshoot into cusps or self-loops.
  const float dt0 = std::max(std::sqrt(glm::distance(p0, p1)), kEpsilon);
  const float dt1 = std::max(std::sqrt(glm::distance(p1, p2)), kEpsilon);
  const float dt2 = std::max(std::sqrt(glm::distance(p2, p3)), kEpsilon);

  // Estimate C^1 endpoint velocities v1 = C'(t1) and v2 = C'(t2) as the
  // knot-weighted blend of neighboring chord velocities (Lagrange derivative
  // across each 3-point stencil).
  const glm::vec3 v1 =
      (dt1 * (p1 - p0) / dt0 + dt0 * (p2 - p1) / dt1) / (dt0 + dt1);
  const glm::vec3 v2 =
      (dt2 * (p2 - p1) / dt1 + dt1 * (p3 - p2) / dt2) / (dt1 + dt2);

  // Convert Hermite endpoints (p1, v1, p2, v2) over t in [t1, t2] of length
  // dt1 into equivalent cubic Bezier control points over u in [0, 1], since
  // B'(0) = 3 * (b1 - b0) = dt1 * v1 and B'(1) = 3 * (b3 - b2) = dt1 * v2.
  const glm::vec3 b0 = p1;
  const glm::vec3 b1 = p1 + (dt1 / 3.0f) * v1;
  const glm::vec3 b2 = p2 - (dt1 / 3.0f) * v2;
  const glm::vec3 b3 = p2;
  BezierSpline::Tessellate(b0, b1, b2, b3, /*recursion_depth=*/0);
}

void CatmullRomSpline::BuildSpline(std::span<const glm::vec3> control_points) {
  const auto num_control_points = static_cast<int>(control_points.size());
  ASSERT_TRUE(
      num_control_points >= kMinNumControlPoints,
      std::format("Must have at least {} control points, while {} provided",
                  kMinNumControlPoints, num_control_points));

  mutable_splines()->clear();
  for (int i = 0; i < num_control_points; ++i) {
    Tessellate(control_points[(i + 0) % num_control_points],
               control_points[(i + 1) % num_control_points],
               control_points[(i + 2) % num_control_points],
               control_points[(i + 3) % num_control_points]);
  }
  // Close the spline.
  mutable_splines()->push_back(spline_points()[0]);
}

SplineEditor::SplineEditor(int min_num_control_points,
                           int max_num_control_points,
                           std::vector<glm::vec3>&& initial_control_points,
                           std::unique_ptr<Spline>&& spline)
    : min_num_control_points_{min_num_control_points},
      max_num_control_points_{max_num_control_points},
      control_points_{std::move(initial_control_points)},
      spline_{std::move(spline)} {
  RebuildSpline();
}

bool SplineEditor::CanInsertControlPoint() const {
  return control_points_.size() != max_num_control_points_;
}

bool SplineEditor::InsertControlPoint(int index, const glm::vec3& position) {
  if (!CanInsertControlPoint()) {
    return false;
  }

  control_points_.insert(control_points_.begin() + index, position);
  RebuildSpline();
  return true;
}

void SplineEditor::UpdateControlPoint(int index, const glm::vec3& position) {
  control_points_.at(index) = position;
  RebuildSpline();
}

bool SplineEditor::RemoveControlPoint(int index) {
  if (control_points_.size() == min_num_control_points_) {
    return false;
  }

  control_points_.erase(control_points_.begin() + index);
  RebuildSpline();
  return true;
}

void SplineEditor::RebuildSpline() { spline_->BuildSpline(control_points_); }

}  // namespace lighter::scene
