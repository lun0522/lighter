//
//  rotation.cc
//
//  Created by Pujun Lun on 11/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/rotation.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "third_party/glm/gtx/vector_angle.hpp"

namespace jessie_steamer {
namespace common {
namespace {

constexpr float kRotationAngleThreshold = 3e-3;
constexpr float kInertialRotationCoeff = 1.5f;

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
  if (state.rotation.angle <= kRotationAngleThreshold ||
      elapsed_time > kInertialRotationCoeff) {
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
    const float angle = glm::angle(state.first_click_pos,
                                   normalized_click_pos.value());

    // If the user is still clicking, and the click position generates a large
    // enough rotation angle, perform rotation. If the rotation angle is too
    // small, keep in rotation state but do not perform rotation.
    if (angle > kRotationAngleThreshold) {
      state.rotation.angle = angle;
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
} /* namespace common */
} /* namespace jessie_steamer */
