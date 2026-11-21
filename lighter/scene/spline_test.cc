//
//  spline_test.cc
//

#include "lighter/scene/spline.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <numbers>
#include <ranges>
#include <vector>

namespace lighter::scene {
namespace {

glm::vec3 PointOnEquator(float angle_rad, float z = 0.0f) {
  return glm::normalize(glm::vec3{std::cos(angle_rad), z, std::sin(angle_rad)});
}

TEST(CatmullRomSplineTest, InterpolatesControlPointsAndClosesSphereLoop) {
  const std::vector<glm::vec3> controls{
      PointOnEquator(0.0f, 0.2f),
      PointOnEquator(0.5f * std::numbers::pi_v<float>, -0.2f),
      PointOnEquator(std::numbers::pi_v<float>, 0.3f),
      PointOnEquator(1.5f * std::numbers::pi_v<float>, -0.1f),
  };

  auto spline = CatmullRomSpline::GetOnSphereSpline(
      /*max_recursion_depth=*/20, /*roughness=*/1E-2f);
  spline->BuildSpline(controls);

  const auto& points = spline->spline_points();
  ASSERT_GT(points.size(), controls.size());
  EXPECT_LT(glm::distance(points.front(), points.back()), 1E-6f);

  for (const glm::vec3& point : points) {
    EXPECT_TRUE(std::isfinite(point.x));
    EXPECT_TRUE(std::isfinite(point.y));
    EXPECT_TRUE(std::isfinite(point.z));
    EXPECT_NEAR(glm::length(point), 1.0f, 1E-5f);
  }

  for (const glm::vec3& control : controls) {
    const float min_dist = std::ranges::min(
        points | std::views::transform([&control](const glm::vec3& p) {
          return glm::distance(p, control);
        }));
    EXPECT_LT(min_dist, 1E-5f);
  }
}

TEST(CatmullRomSplineTest, CloseControlPointsAvoidCuspsAndLoops) {
  // Place two anchors very close together (~0.025 rad apart) between much
  // farther anchors. Uniform Catmull-Rom produces a cusp or loop here; the
  // centripetal variant keeps turning angles small.
  const std::vector<glm::vec3> controls{
      PointOnEquator(0.0f, 0.1f),
      PointOnEquator(1.40f, 0.15f),
      PointOnEquator(1.425f, 0.15f),
      PointOnEquator(3.1f, -0.1f),
  };

  auto spline = CatmullRomSpline::GetOnSphereSpline(
      /*max_recursion_depth=*/20, /*roughness=*/1E-2f);
  spline->BuildSpline(controls);

  const auto& points = spline->spline_points();
  ASSERT_GT(points.size(), 10u);

  float max_turn_angle = 0.0f;
  for (size_t i = 0; i + 2 < points.size(); ++i) {
    const glm::vec3 d0 = points[i + 1] - points[i];
    const glm::vec3 d1 = points[i + 2] - points[i + 1];
    if (glm::length(d0) < 1E-5f || glm::length(d1) < 1E-5f) {
      continue;
    }
    const float turn = glm::angle(glm::normalize(d0), glm::normalize(d1));
    EXPECT_TRUE(std::isfinite(turn));
    max_turn_angle = std::max(max_turn_angle, turn);
  }
  EXPECT_LT(max_turn_angle, 0.2f);
}

TEST(CatmullRomSplineTest, CoincidentControlPointsProduceFinitePoints) {
  const std::vector<glm::vec3> partial_duplicate{
      PointOnEquator(0.0f, 0.2f),
      PointOnEquator(1.2f, 0.1f),
      PointOnEquator(1.2f, 0.1f),
      PointOnEquator(2.6f, -0.2f),
  };

  auto spline = CatmullRomSpline::GetOnSphereSpline(
      /*max_recursion_depth=*/20, /*roughness=*/1E-2f);
  spline->BuildSpline(partial_duplicate);

  ASSERT_FALSE(spline->spline_points().empty());
  for (const glm::vec3& point : spline->spline_points()) {
    EXPECT_TRUE(std::isfinite(point.x));
    EXPECT_TRUE(std::isfinite(point.y));
    EXPECT_TRUE(std::isfinite(point.z));
    EXPECT_NEAR(glm::length(point), 1.0f, 1E-5f);
  }

  const std::vector<glm::vec3> all_identical(3, PointOnEquator(0.5f, 0.0f));
  spline->BuildSpline(all_identical);
  ASSERT_FALSE(spline->spline_points().empty());
  for (const glm::vec3& point : spline->spline_points()) {
    EXPECT_TRUE(std::isfinite(point.x));
    EXPECT_NEAR(glm::length(point), 1.0f, 1E-5f);
  }
}

TEST(SplineEditorTest, EnforcesBoundsAndRebuildsSpline) {
  std::vector<glm::vec3> initial_controls{
      PointOnEquator(0.0f),
      PointOnEquator(1.5f),
      PointOnEquator(3.0f),
      PointOnEquator(4.5f),
  };

  SplineEditor editor{
      /*min_num_control_points=*/3,
      /*max_num_control_points=*/5,
      std::move(initial_controls),
      CatmullRomSpline::GetOnSphereSpline(
          /*max_recursion_depth=*/20, /*roughness=*/1E-2f),
  };

  EXPECT_TRUE(editor.CanInsertControlPoint());
  const glm::vec3 inserted = PointOnEquator(5.2f, 0.3f);
  EXPECT_TRUE(editor.InsertControlPoint(4, inserted));
  EXPECT_FALSE(editor.CanInsertControlPoint());
  EXPECT_FALSE(editor.InsertControlPoint(0, PointOnEquator(0.4f)));
  EXPECT_EQ(editor.control_points().size(), 5u);

  const glm::vec3 updated = PointOnEquator(1.8f, -0.25f);
  editor.UpdateControlPoint(1, updated);
  const float updated_dist =
      std::ranges::min(editor.spline_points() |
                       std::views::transform([&updated](const glm::vec3& p) {
                         return glm::distance(p, updated);
                       }));
  EXPECT_LT(updated_dist, 1E-5f);

  EXPECT_TRUE(editor.RemoveControlPoint(4));
  EXPECT_TRUE(editor.RemoveControlPoint(0));
  EXPECT_FALSE(editor.RemoveControlPoint(0));
  EXPECT_EQ(editor.control_points().size(), 3u);
}

TEST(SplineEditorTest, FindClosestInsertionIndexSplitsIntendedSegment) {
  // P0 (0.0) and P1 (0.15) are close together while P2 (1.6) is much farther.
  // Clicking at 0.25 lies between P1 and P2 even though it is closer to P0
  // than to P2; insertion must still split P1 -> P2 (index 2).
  std::vector<glm::vec3> controls{
      PointOnEquator(0.0f),
      PointOnEquator(0.15f),
      PointOnEquator(1.6f),
      PointOnEquator(3.2f),
  };

  SplineEditor editor{
      /*min_num_control_points=*/3,
      /*max_num_control_points=*/8,
      std::move(controls),
      CatmullRomSpline::GetOnSphereSpline(
          /*max_recursion_depth=*/20, /*roughness=*/1E-2f),
  };

  const glm::vec3 click_between_1_and_2 = PointOnEquator(0.25f);
  EXPECT_EQ(editor.FindClosestInsertionIndex(click_between_1_and_2), 2);

  // Clicking near the closing wraparound segment P3 -> P0 inserts at index 0
  // (splitting P3 -> P0).
  const glm::vec3 click_between_3_and_0 = PointOnEquator(4.5f);
  EXPECT_EQ(editor.FindClosestInsertionIndex(click_between_3_and_0), 0);

  // When a visibility filter hides the P1 -> P2 region, only visible segments
  // are considered.
  const auto front_only = [](const glm::vec3& p) { return p.z < 0.0f; };
  const std::optional<int> visible_index =
      editor.FindClosestInsertionIndex(click_between_1_and_2, front_only);
  ASSERT_TRUE(visible_index.has_value());
  EXPECT_NE(*visible_index, 2);
}

}  // namespace
}  // namespace lighter::scene
