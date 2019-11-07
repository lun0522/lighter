//
//  rotation.cc
//
//  Created by Pujun Lun on 11/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/rotation.h"

#include "jessie_steamer/common/util.h"
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
  if (normalized_click_pos.has_value()) {
    rotation_manager->state_ = RotationManager::RotationState{
      normalized_click_pos.value(), Rotation{}};
  }
  return absl::nullopt;
}

template <>
absl::optional<Rotation> Compute<RotationManager::RotationState>(
    const absl::optional<glm::vec3>& normalized_click_pos,
    RotationManager* rotation_manager) {
  auto& state = absl::get<RotationManager::RotationState>(
      rotation_manager->state_);
  if (normalized_click_pos.has_value()) {
    const float angle = glm::angle(state.last_click_pos,
                                   normalized_click_pos.value());
    if (angle > kRotationAngleThreshold) {
      state.rotation.angle = angle;
      state.rotation.axis = glm::cross(state.last_click_pos,
                                       normalized_click_pos.value());
      return state.rotation;
    } else {
      state.rotation.angle = 0.0f;
      return absl::nullopt;
    }
  } else {
    rotation_manager->state_ = RotationManager::InertialRotationState{
        rotation_manager->timer_.GetElapsedTimeSinceLaunch(), state.rotation};
    return absl::nullopt;
  }
}

template <>
absl::optional<Rotation> Compute<RotationManager::InertialRotationState>(
    const absl::optional<glm::vec3>& normalized_click_pos,
    RotationManager* rotation_manager) {
  if (normalized_click_pos.has_value()) {
    rotation_manager->state_ = RotationManager::RotationState{
      normalized_click_pos.value(), Rotation{}};
    return absl::nullopt;
  }

  const auto& state = absl::get<RotationManager::InertialRotationState>(
      rotation_manager->state_);
  const float elapsed_time =
      rotation_manager->timer_.GetElapsedTimeSinceLaunch() - state.start_time;
  if (state.rotation.angle == 0.0f || elapsed_time > kInertialRotationCoeff) {
    rotation_manager->state_ = RotationManager::StopState{};
    return absl::nullopt;
  } else {
    const float fraction =
        1.0f - std::pow(elapsed_time / kInertialRotationCoeff, 2.0f);
    return Rotation{state.rotation.axis, state.rotation.angle * fraction};
  }
}

} /* namespace rotation */
} /* namespace common */
} /* namespace jessie_steamer */
