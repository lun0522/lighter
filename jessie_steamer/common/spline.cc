//
//  spline.cc
//
//  Created by Pujun Lun on 11/30/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/spline.h"

#include "jessie_steamer/common/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/glm/gtx/vector_angle.hpp"

namespace jessie_steamer {
namespace common {
namespace {

using std::vector;

} /* namespace */

BezierSpline::BezierSpline(int max_recursion_depth,
                           GetMiddlePoint&& get_middle_point,
                           IsSmooth&& is_smooth,
                           PostProcessing&& post_processing)
    : max_recursion_depth_{max_recursion_depth},
      get_middle_point_{std::move(get_middle_point)},
      is_smooth_{std::move(is_smooth)},
      post_processing_{std::move(post_processing)} {
  ASSERT_NON_NULL(get_middle_point_, "get_middle_point must not be nullptr");
  ASSERT_NON_NULL(is_smooth_, "is_smooth must not be nullptr");
}

void BezierSpline::Tessellate(const glm::vec3& p0,
                              const glm::vec3& p1,
                              const glm::vec3& p2,
                              const glm::vec3& p3,
                              int recursion_depth) {
  constexpr float kMinDistBetweenPoints = 1E-2;
  if (++recursion_depth == max_recursion_depth_ ||
      glm::distance(p0, p3) < kMinDistBetweenPoints ||
      is_smooth_(p0, p1, p2, p3)) {
    spline_points_.emplace_back(
        post_processing_ == nullptr ? p0 : post_processing_(p0));
    return;
  }

  const glm::vec3 p10 = get_middle_point_(p0, p1);
  const glm::vec3 p11 = get_middle_point_(p1, p2);
  const glm::vec3 p12 = get_middle_point_(p2, p3);
  const glm::vec3 p20 = get_middle_point_(p10, p11);
  const glm::vec3 p21 = get_middle_point_(p11, p12);
  const glm::vec3 p30 = get_middle_point_(p20, p21);
  Tessellate(p0, p10, p20, p30, recursion_depth);
  Tessellate(p30, p21, p12, p3, recursion_depth);
}

void BezierSpline::BuildSpline(const vector<glm::vec3>& control_points) {
  FATAL("Unimplemented");
}

std::unique_ptr<Spline> CatmullRomSpline::GetOnSphereSpline(
    const glm::vec3& sphere_center, float sphere_radius,
    int max_recursion_depth, float smoothness) {
  // Middle point calculation based on Slerp:
  // https://en.wikipedia.org/wiki/Slerp
  BezierSpline::GetMiddlePoint get_middle_point =
      [&sphere_center](const glm::vec3& p0, const glm::vec3& p1) {
        const float cos_omega = glm::dot(glm::normalize(p0 - sphere_center),
                                         glm::normalize(p1 - sphere_center));
        const float omega = glm::acos(cos_omega);
        const float interp = glm::sin(0.5f * omega) / glm::sin(omega);
        // middle_point = center + interp * (p0 - center)
        //                       + interp * (p1 - center)
        //              = center + interp * (p0 + p1 - center * 2)
        return sphere_center + interp * (p0 + p1 - sphere_center * 2.0f);
      };

  BezierSpline::IsSmooth is_smooth = [smoothness](const glm::vec3& p0,
                                                  const glm::vec3& p1,
                                                  const glm::vec3& p2,
                                                  const glm::vec3& p3) {
    const glm::vec3 p0p1 = glm::normalize(p0 - p1);
    const glm::vec3 p1p2 = glm::normalize(p1 - p2);
    const glm::vec3 p2p3 = glm::normalize(p2 - p3);
    return glm::angle(p0p1, p1p2) <= smoothness &&
           glm::angle(p1p2, p2p3) <= smoothness;
  };

  BezierSpline::PostProcessing post_processing =
      [&sphere_center, sphere_radius](const glm::vec3& point) {
        return sphere_center + (point - sphere_center) * sphere_radius;
      };

  return absl::make_unique<CatmullRomSpline>(
      max_recursion_depth, std::move(get_middle_point), std::move(is_smooth),
      std::move(post_processing));
}

void CatmullRomSpline::Tessellate(const glm::vec3& p0,
                                  const glm::vec3& p1,
                                  const glm::vec3& p2,
                                  const glm::vec3& p3) {
  static const glm::mat4* catmull_rom_to_bezier = nullptr;
  if (catmull_rom_to_bezier == nullptr) {
    const glm::mat4 catmull_rom_coeff{
        -0.5f,  1.5f, -1.5f,  0.5f,
         1.0f, -2.5f,  2.0f, -0.5f,
        -0.5f,  0.0f,  0.5f,  0.0f,
         0.0f,  1.0f,  0.0f,  0.0f
    };
    const glm::mat4 bezier_coeff{
        -1.0f,  3.0f, -3.0f,  1.0f,
         3.0f, -6.0f,  3.0f,  0.0f,
        -3.0f,  3.0f,  0.0f,  0.0f,
         1.0f,  0.0f,  0.0f,  0.0f,
    };
    catmull_rom_to_bezier =
        new glm::mat4{catmull_rom_coeff * glm::inverse(bezier_coeff)};
  }
  const glm::mat4 catmull_rom_points{
      glm::vec4{p0, 0.0f},
      glm::vec4{p1, 0.0f},
      glm::vec4{p2, 0.0f},
      glm::vec4{p3, 0.0f},
  };
  const glm::mat4 bezier_points = catmull_rom_points * (*catmull_rom_to_bezier);
  bezier_spline_.Tessellate(bezier_points[0],
                            bezier_points[1],
                            bezier_points[2],
                            bezier_points[3],
                            /*recursion_depth=*/0);
}

void CatmullRomSpline::BuildSpline(const vector<glm::vec3>& control_points) {
  const auto num_control_points = static_cast<int>(control_points.size());
  if (num_control_points < kMinNumControlPoints) {
    FATAL(absl::StrFormat(
        "Must have at least %d control points, while %d provided",
        kMinNumControlPoints, num_control_points));
  }

  spline_points_.clear();
  for (int i = 0; i < num_control_points; ++i) {
    Tessellate(control_points[(i + 0) % num_control_points],
               control_points[(i + 1) % num_control_points],
               control_points[(i + 2) % num_control_points],
               control_points[(i + 3) % num_control_points]);
  }
  spline_points_.emplace_back(spline_points_[0]);  // Close the spline.
}

SplineEditor::SplineEditor(int min_num_control_points,
                           int max_num_control_points,
                           vector<glm::vec3>&& initial_control_points,
                           std::unique_ptr<Spline>&& spline)
    : min_num_control_points_{min_num_control_points},
      max_num_control_points_{max_num_control_points},
      control_points_{std::move(initial_control_points)},
      spline_{std::move(spline)} {
  RebuildSpline();
}

absl::optional<int> SplineEditor::FindClickedControlPoint(
    const glm::vec3& click_pos, float max_distance) {
  for (int i = 0; i < control_points_.size(); ++i) {
    if (glm::distance(control_points_[i], click_pos) <= max_distance) {
      return i;
    }
  }
  return absl::nullopt;
}

void SplineEditor::AddControlPoint(const glm::vec3& position) {
  // TODO
}

void SplineEditor::RemoveControlPoint(int index) {
  control_points_.erase(control_points_.begin() + index);
  RebuildSpline();
}

void SplineEditor::RebuildSpline() {
  spline_->BuildSpline(control_points_);
}

} /* namespace common */
} /* namespace jessie_steamer */
