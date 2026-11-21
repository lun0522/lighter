//
//  rotation_test.cc
//

#include "lighter/scene/rotation.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <glm/glm.hpp>
#include <numbers>
#include <thread>

namespace lighter::scene {
namespace {

TEST(RotationManagerTest, TransitionsAcrossStopDragInertialAndReclickStates) {
  RotationManager manager{/*inertial_rotation_duration=*/10.0f};

  // StopState without click stays idle.
  EXPECT_FALSE(manager.Compute(std::nullopt).has_value());

  // Initial click enters RotationState without rotating yet.
  const glm::vec3 p0 = glm::normalize(glm::vec3{0.0f, 0.0f, 1.0f});
  EXPECT_FALSE(manager.Compute(p0).has_value());

  // Repeating identical click position emits no rotation and clears delta.
  EXPECT_FALSE(manager.Compute(p0).has_value());

  // Dragging to a new point computes axis and angle from first_click_pos.
  const glm::vec3 p1 = glm::normalize(glm::vec3{1.0f, 0.0f, 0.0f});
  const auto drag_rot = manager.Compute(p1);
  ASSERT_TRUE(drag_rot.has_value());
  EXPECT_NEAR(drag_rot->angle, 0.5f * std::numbers::pi_v<float>, 1E-4f);
  EXPECT_NEAR(glm::distance(drag_rot->axis, glm::vec3{0.0f, 1.0f, 0.0f}), 0.0f,
              1E-5f);

  // Releasing transitions to InertialRotationState and decays rotation.
  const auto inertial_rot = manager.Compute(std::nullopt);
  ASSERT_TRUE(inertial_rot.has_value());
  EXPECT_GT(inertial_rot->angle, 0.0f);
  EXPECT_LE(inertial_rot->angle, drag_rot->angle);

  // Clicking again during inertial rotation re-enters RotationState cleanly.
  EXPECT_FALSE(manager.Compute(p0).has_value());
}

TEST(RotationManagerTest, InertialRotationExpiresAfterDuration) {
  RotationManager manager{/*inertial_rotation_duration=*/0.005f};
  const glm::vec3 p0{0.0f, 0.0f, 1.0f};
  const glm::vec3 p1{1.0f, 0.0f, 0.0f};

  EXPECT_FALSE(manager.Compute(p0).has_value());
  ASSERT_TRUE(manager.Compute(p1).has_value());

  std::this_thread::sleep_for(std::chrono::milliseconds(15));
  EXPECT_FALSE(manager.Compute(std::nullopt).has_value());
}

TEST(CameraViewedSphereTest, IntersectsPerspectiveAndOrthographicClicks) {
  PerspectiveCamera persp_camera{
      Camera::Config{
          .near = 0.1f,
          .far = 100.0f,
          .position = {0.0f, 0.0f, 5.0f},
          .look_at = {0.0f, 0.0f, 0.0f},
      },
      PerspectiveCamera::FrustumConfig{
          .field_of_view_y = 60.0f,
          .aspect_ratio = 1.0f,
      },
  };
  PerspectiveCameraViewedSphere persp_sphere{
      /*center=*/glm::vec3{0.0f}, /*radius=*/1.0f,
      /*inertial_rotation_duration=*/1.0f};

  const auto persp_hit =
      persp_sphere.GetIntersection(persp_camera, {0.0f, 0.0f});
  ASSERT_TRUE(persp_hit.has_value());
  EXPECT_NEAR(persp_hit->z, 1.0f, 1E-4f);
  EXPECT_FALSE(
      persp_sphere.GetIntersection(persp_camera, {0.95f, 0.95f}).has_value());

  OrthographicCamera ortho_camera{
      Camera::Config{
          .near = 0.1f,
          .far = 100.0f,
          .position = {0.0f, 0.0f, 5.0f},
          .look_at = {0.0f, 0.0f, 0.0f},
      },
      OrthographicCamera::OrthoConfig{
          .view_width = 6.0f,
          .aspect_ratio = 1.0f,
      },
  };
  OrthographicCameraViewedSphere ortho_sphere{
      /*center=*/glm::vec3{2.0f, 0.0f, 0.0f}, /*radius=*/2.0f,
      /*inertial_rotation_duration=*/1.0f};

  // Skybox matrix strips translation and scales uniformly by 'scale'.
  const glm::mat4 skybox = ortho_sphere.GetSkyboxModelMatrix(3.0f);
  EXPECT_NEAR(skybox[3][0], 0.0f, 1E-6f);
  EXPECT_NEAR(glm::length(glm::vec3{skybox[0]}), 3.0f, 1E-5f);

  const auto ortho_hit =
      ortho_sphere.GetIntersection(ortho_camera, {2.0f / 3.0f, 0.0f});
  ASSERT_TRUE(ortho_hit.has_value());
  EXPECT_NEAR(glm::length(*ortho_hit), 1.0f, 1E-4f);
  EXPECT_FALSE(
      ortho_sphere.GetIntersection(ortho_camera, {-0.9f, 0.0f}).has_value());
}

}  // namespace
}  // namespace lighter::scene
