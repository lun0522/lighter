//
//  camera_test.cc
//

#include "lighter/scene/camera.h"

#include <gtest/gtest.h>

#include <cmath>
#include <glm/glm.hpp>

namespace lighter::scene {
namespace {

TEST(CameraTest, MaintainsOrthonormalBasisAndMatrices) {
  PerspectiveCamera camera{
      Camera::Config{
          .near = 0.5f,
          .far = 50.0f,
          .up = {0.0f, 2.0f, 0.0f},
          .position = {0.0f, 0.0f, 5.0f},
          .look_at = {0.0f, 0.0f, 0.0f},
      },
      PerspectiveCamera::FrustumConfig{
          .field_of_view_y = 60.0f,
          .aspect_ratio = 2.0f,
      },
  };

  EXPECT_NEAR(glm::length(camera.up()), 1.0f, 1E-5f);
  EXPECT_NEAR(glm::length(camera.front()), 1.0f, 1E-5f);
  EXPECT_NEAR(glm::length(camera.right()), 1.0f, 1E-5f);
  EXPECT_NEAR(glm::dot(camera.front(), camera.right()), 0.0f, 1E-5f);
  EXPECT_NEAR(glm::dot(camera.up(), camera.right()), 0.0f, 1E-5f);

  // Verify Vulkan [0, 1] depth mapping via GLM_FORCE_DEPTH_ZERO_TO_ONE.
  const glm::mat4 vp = camera.GetProjectionMatrix() * camera.GetViewMatrix();
  const glm::vec4 near_clip = vp * glm::vec4{0.0f, 0.0f, 4.5f, 1.0f};
  const glm::vec4 far_clip = vp * glm::vec4{0.0f, 0.0f, -45.0f, 1.0f};
  EXPECT_NEAR(near_clip.z / near_clip.w, 0.0f, 1E-5f);
  EXPECT_NEAR(far_clip.z / far_clip.w, 1.0f, 1E-4f);

  // Skybox view matrix preserves rotation but zeroes translation.
  const glm::mat4 skybox_view = camera.GetSkyboxViewMatrix();
  EXPECT_NEAR(skybox_view[3][0], 0.0f, 1E-6f);
  EXPECT_NEAR(skybox_view[3][1], 0.0f, 1E-6f);
  EXPECT_NEAR(skybox_view[3][2], 0.0f, 1E-6f);

  const auto ray_params = camera.GetRayTracingParams();
  EXPECT_NEAR(glm::length(ray_params.up), std::tan(glm::radians(60.0f)), 1E-5f);
  EXPECT_NEAR(glm::length(ray_params.right),
              2.0f * std::tan(glm::radians(60.0f)), 1E-5f);
}

TEST(CameraTest, OrthographicProjectionMapsExtentsToNdc) {
  OrthographicCamera camera{
      Camera::Config{
          .near = 1.0f,
          .far = 11.0f,
          .up = {0.0f, 1.0f, 0.0f},
          .position = {0.0f, 0.0f, 0.0f},
          .look_at = {0.0f, 0.0f, -1.0f},
      },
      OrthographicCamera::OrthoConfig{
          .view_width = 8.0f,
          .aspect_ratio = 2.0f,
      },
  };

  const glm::mat4 vp = camera.GetProjectionMatrix() * camera.GetViewMatrix();
  const glm::vec4 top_right_near = vp * glm::vec4{4.0f, 2.0f, -1.0f, 1.0f};
  EXPECT_NEAR(top_right_near.x, 1.0f, 1E-5f);
  EXPECT_NEAR(top_right_near.y, 1.0f, 1E-5f);
  EXPECT_NEAR(top_right_near.z, 0.0f, 1E-5f);

  camera.SetViewWidth(4.0f);
  EXPECT_FLOAT_EQ(camera.view_width(), 4.0f);
}

TEST(UserControlledCameraTest, ScrollKeyMovementAndPitchClamping) {
  auto controlled = UserControlledPerspectiveCamera::Create(
      camera_control::Config{
          .move_speed = 2.0f,
          .turn_speed = 0.01f,
          .lock_center = glm::vec3{0.0f},
      },
      Camera::Config{
          .position = {0.0f, 0.0f, 5.0f},
          .look_at = {0.0f, 0.0f, 0.0f},
      },
      PerspectiveCamera::FrustumConfig{
          .field_of_view_y = 45.0f,
          .aspect_ratio = 1.0f,
      });

  // Inactive camera ignores input.
  EXPECT_FALSE(controlled->DidScroll(-5.0, 20.0, 90.0));
  controlled->DidPressKey(camera_control::Key::kRight, 1.0f);
  EXPECT_NEAR(controlled->camera().position().x, 0.0f, 1E-6f);

  controlled->SetActivity(true);
  EXPECT_TRUE(controlled->DidScroll(-10.0, 20.0, 90.0));
  EXPECT_FLOAT_EQ(controlled->camera().field_of_view_y(), 35.0f);
  EXPECT_TRUE(controlled->DidScroll(-50.0, 20.0, 90.0));
  EXPECT_FLOAT_EQ(controlled->camera().field_of_view_y(), 20.0f);
  EXPECT_FALSE(controlled->DidScroll(-5.0, 20.0, 90.0));

  // Orbit movement preserves distance to lock_center.
  controlled->DidPressKey(camera_control::Key::kRight, 0.25f);
  EXPECT_GT(controlled->camera().position().x, 0.0f);
  EXPECT_NEAR(glm::length(controlled->camera().position()), 5.0f, 1E-5f);

  // Huge vertical cursor drag clamps pitch below 90 deg so front != up.
  controlled->SetCursorPos({0.0, 0.0});
  controlled->DidMoveCursor(0.0, -100000.0);
  EXPECT_LT(glm::dot(controlled->camera().front(), controlled->camera().up()),
            1.0f);
  EXPECT_NEAR(glm::length(controlled->camera().front()), 1.0f, 1E-5f);
}

}  // namespace
}  // namespace lighter::scene
