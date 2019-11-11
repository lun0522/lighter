//
//  rotation.cc
//
//  Created by Pujun Lun on 11/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/rotation.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "third_party/glm/gtx/intersect.hpp"
#include "third_party/glm/gtx/vector_angle.hpp"

namespace jessie_steamer {
namespace common {
namespace {

constexpr float kInertialRotationCoeff = 1.5f;

// Applies 'transform' to a 3D 'point'.
glm::vec3 TransformPoint(const glm::mat4& transform, const glm::vec3& point) {
  const glm::vec4 transformed = transform * glm::vec4{point, 1.0f};
  return glm::vec3{transformed} / transformed.w;
}

} /* namespace */

namespace rotation {

template <>
absl::optional<Rotation> Compute<RotationManager::StopState>(
    const absl::optional<glm::vec3>& normalized_click_pos,
    RotationManager* rotation_manager) {
  // If there is any clicking, turn to rotation state. The rotation axis and
  // angle are left for rotation state to compute.
  if (normalized_click_pos.has_value()) {
    rotation_manager->state_ = RotationManager::RotationState{
        /*last_click_time=*/rotation_manager->GetReferenceTime(),
        /*first_click_pos=*/normalized_click_pos.value(), Rotation{}};
  }

  // No rotation should be performed this time.
  return absl::nullopt;
}

template <>
absl::optional<Rotation> Compute<RotationManager::InertialRotationState>(
    const absl::optional<glm::vec3>& normalized_click_pos,
    RotationManager* rotation_manager) {
  // If there is any clicking, turn to rotation state. The rotation axis and
  // angle are left for rotation state to compute.
  if (normalized_click_pos.has_value()) {
    rotation_manager->state_ = RotationManager::RotationState{
        /*last_click_time=*/rotation_manager->GetReferenceTime(),
        /*first_click_pos=*/normalized_click_pos.value(), Rotation{}};
    return absl::nullopt;
  }

  const auto& state = absl::get<RotationManager::InertialRotationState>(
      rotation_manager->state_);
  const float elapsed_time =
      rotation_manager->GetReferenceTime() - state.start_time;

  // Otherwise, if rotation angle is large enough, keep rotating at decreasing
  // speed, and stop after 'kInertialRotationCoeff' seconds.
  if (state.rotation.angle == 0.0f || elapsed_time > kInertialRotationCoeff) {
    rotation_manager->state_ = RotationManager::StopState{};
    return absl::nullopt;
  } else {
    const float fraction =
        1.0f - std::pow(elapsed_time / kInertialRotationCoeff, 2.0f);
    return Rotation{state.rotation.axis, state.rotation.angle * fraction};
  }
}

template <>
absl::optional<Rotation> Compute<RotationManager::RotationState>(
    const absl::optional<glm::vec3>& normalized_click_pos,
    RotationManager* rotation_manager) {
  if (normalized_click_pos.has_value()) {
    auto& state = absl::get<RotationManager::RotationState>(
        rotation_manager->state_);
    state.last_click_time = rotation_manager->GetReferenceTime();
    // If the user is clicking on a different position, perform rotation.
    // Otherwise, keep in rotation state but do not perform rotation.
    if (state.first_click_pos != normalized_click_pos.value()) {
      state.rotation.angle = glm::angle(state.first_click_pos,
                                        normalized_click_pos.value());
      state.rotation.axis = glm::cross(state.first_click_pos,
                                       normalized_click_pos.value());
      return state.rotation;
    } else {
      state.rotation.angle = 0.0f;
      return absl::nullopt;
    }
  } else {
    // If the user is no longer clicking, turn to inertial rotation state.
    // It is up to inertial rotation state whether or not to perform rotation
    // this time.
    const auto& state = absl::get<RotationManager::RotationState>(
        rotation_manager->state_);
    rotation_manager->state_ = RotationManager::InertialRotationState{
        /*start_time=*/state.last_click_time, state.rotation};
    return Compute<RotationManager::InertialRotationState>(
        /*normalized_click_pos=*/absl::nullopt, rotation_manager);
  }
}

} /* namespace rotation */

Sphere::Sphere(const glm::vec3& center, float radius)
    : center_{center}, radius_{radius}, model_matrix_{1.0f} {
  // Initially, the north pole points to the center of frame.
  model_matrix_ = glm::translate(model_matrix_, center_);
  model_matrix_ = glm::rotate(model_matrix_, glm::radians(-90.0f),
                              glm::vec3(1.0f, 0.0f, 0.0f));
  model_matrix_ = glm::rotate(model_matrix_, glm::radians(-90.0f),
                              glm::vec3(0.0f, 1.0f, 0.0f));
  model_matrix_ = glm::scale(model_matrix_, glm::vec3{radius_});
}

absl::optional<glm::vec3> Sphere::GetIntersection(
    const common::Camera& camera, const glm::vec2& click_ndc) const {
  // All computation will be done in the object space.
  const glm::mat4 world_to_object = glm::inverse(model_matrix_);
  const glm::mat4 world_to_ndc = camera.projection() * camera.view();
  const glm::mat4 ndc_to_world = glm::inverse(world_to_ndc);
  const glm::mat4 ndc_to_object = world_to_object * ndc_to_world;

  const glm::vec3 camera_pos =
      TransformPoint(world_to_object, camera.position());
  // Z value does not matter since this is in the NDC space.
  const glm::vec3 click_pos =
      TransformPoint(ndc_to_object, glm::vec3{click_ndc, 1.0f});

  glm::vec3 position, normal;
  if (glm::intersectRaySphere(
          camera_pos, glm::normalize(click_pos - camera_pos),
          /*sphereCenter=*/glm::vec3{0.0f}, /*sphereRadius=*/1.0f,
          position, normal)) {
    return position;
  } else {
    return absl::nullopt;
  }
}

void Sphere::Update(const common::Camera& camera,
                    const absl::optional<glm::vec2>& click_ndc) {
  const auto intersection = click_ndc.has_value()
                                ? GetIntersection(camera, click_ndc.value())
                                : absl::nullopt;
  const auto rotation = rotation_manager_.Compute(intersection);
  if (rotation.has_value()) {
    model_matrix_ = glm::rotate(model_matrix_, rotation->angle, rotation->axis);
  }
}

glm::mat4 Sphere::GetSkyboxModelMatrix() const {
  glm::mat4 skybox_model = glm::scale(model_matrix_, glm::vec3{1.0f / radius_});
  skybox_model[3] *= glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
  return skybox_model;
}

} /* namespace common */
} /* namespace jessie_steamer */
