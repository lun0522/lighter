//
//  rotation.cc
//
//  Created by Pujun Lun on 11/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/common/rotation.h"

#include <type_traits>

#include "lighter/common/util.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "third_party/glm/gtx/intersect.hpp"
#include "third_party/glm/gtx/vector_angle.hpp"

namespace lighter::common {
namespace {

// Applies 'transform' to a 3D 'point'.
inline glm::vec3 TransformPoint(const glm::mat4& transform,
                                const glm::vec3& point) {
  const glm::vec4 transformed = transform * glm::vec4{point, 1.0f};
  return glm::vec3{transformed} / transformed.w;
}

// Applies 'transform' to a 3D 'vector'.
inline glm::vec3 TransformVector(const glm::mat4& transform,
                                 const glm::vec3& vector) {
  return glm::vec3{transform * glm::vec4{vector, 0.0f}};
}

}  // namespace

namespace rotation {

template <>
std::optional<Rotation> Compute<RotationManager::StopState>(
    const std::optional<glm::vec3>& normalized_click_pos,
    RotationManager* rotation_manager) {
  // If there is any clicking, turn to rotation state. The rotation axis and
  // angle are left for rotation state to compute.
  if (normalized_click_pos.has_value()) {
    rotation_manager->state_ = RotationManager::RotationState{
        .last_click_time = rotation_manager->GetReferenceTime(),
        .first_click_pos = normalized_click_pos.value(),
        .rotation = Rotation{}};
  }

  // No rotation should be performed this time.
  return std::nullopt;
}

template <>
std::optional<Rotation> Compute<RotationManager::InertialRotationState>(
    const std::optional<glm::vec3>& normalized_click_pos,
    RotationManager* rotation_manager) {
  // If there is any clicking, turn to rotation state. The rotation axis and
  // angle are left for rotation state to compute.
  if (normalized_click_pos.has_value()) {
    rotation_manager->state_ = RotationManager::RotationState{
        .last_click_time = rotation_manager->GetReferenceTime(),
        .first_click_pos = normalized_click_pos.value(),
        .rotation = Rotation{}};
    return std::nullopt;
  }

  const auto& state = std::get<RotationManager::InertialRotationState>(
      rotation_manager->state_);
  const float elapsed_time =
      rotation_manager->GetReferenceTime() - state.start_time;

  // Otherwise, if rotation angle is large enough, keep rotating at decreasing
  // speed, and stop after 'inertial_rotation_duration_' seconds.
  if (state.rotation.angle == 0.0f ||
      elapsed_time > rotation_manager->inertial_rotation_duration_) {
    rotation_manager->state_ = RotationManager::StopState{};
    return std::nullopt;
  } else {
    const float inertial_rotation_process =
        elapsed_time / rotation_manager->inertial_rotation_duration_;
    const float angle = state.rotation.angle *
                        (1.0f - std::pow(inertial_rotation_process, 2.0f));
    return Rotation{state.rotation.axis, angle};
  }
}

template <>
std::optional<Rotation> Compute<RotationManager::RotationState>(
    const std::optional<glm::vec3>& normalized_click_pos,
    RotationManager* rotation_manager) {
  if (normalized_click_pos.has_value()) {
    auto& state = std::get<RotationManager::RotationState>(
        rotation_manager->state_);
    state.last_click_time = rotation_manager->GetReferenceTime();
    // If the user is clicking on a different position, perform rotation.
    // Otherwise, keep in rotation state but do not perform rotation.
    if (state.first_click_pos != normalized_click_pos.value()) {
      state.rotation.angle = glm::angle(state.first_click_pos,
                                        normalized_click_pos.value());
      state.rotation.axis = glm::normalize(
          glm::cross(state.first_click_pos, normalized_click_pos.value()));
      return state.rotation;
    } else {
      state.rotation.angle = 0.0f;
      return std::nullopt;
    }
  } else {
    // If the user is no longer clicking, turn to inertial rotation state.
    // It is up to inertial rotation state whether or not to perform rotation
    // this time.
    const auto& state = std::get<RotationManager::RotationState>(
        rotation_manager->state_);
    rotation_manager->state_ = RotationManager::InertialRotationState{
        .start_time = state.last_click_time, .rotation = state.rotation};
    return Compute<RotationManager::InertialRotationState>(
        /*normalized_click_pos=*/std::nullopt, rotation_manager);
  }
}

}  // namespace rotation

Sphere::Sphere(const glm::vec3& center, float radius,
               float inertial_rotation_duration)
    : radius_{radius}, model_matrix_{1.0f},
      rotation_manager_{inertial_rotation_duration} {
  model_matrix_ = glm::translate(model_matrix_, center);
  model_matrix_ = glm::scale(model_matrix_, glm::vec3{radius_});
}

std::optional<rotation::Rotation> Sphere::ShouldRotate(
    const std::optional<glm::vec3>& intersection) {
  return rotation_manager_.Compute(intersection);
}

void Sphere::Rotate(const rotation::Rotation& rotation) {
  model_matrix_ = glm::rotate(model_matrix_, rotation.angle, rotation.axis);
}

glm::mat4 Sphere::GetSkyboxModelMatrix(float scale) const {
  auto skybox_model = glm::scale(model_matrix_, glm::vec3{scale / radius_});
  skybox_model[3] *= glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
  return skybox_model;
}

template <typename CameraType>
Sphere::Ray CameraViewedSphere<CameraType>::GetClickingRay(
    const CameraType& camera, const glm::vec2& click_ndc) const {
  // All computation will be done in the object space.
  const glm::mat4 world_to_object = glm::inverse(model_matrix());
  const glm::mat4 world_to_ndc = camera.GetProjectionMatrix() *
                                 camera.GetViewMatrix();
  const glm::mat4 ndc_to_world = glm::inverse(world_to_ndc);
  const glm::mat4 ndc_to_object = world_to_object * ndc_to_world;

  if constexpr (std::is_same_v<CameraType, PerspectiveCamera>) {
    const glm::vec3 camera_pos =
        TransformPoint(world_to_object, camera.position());
    constexpr float kFarPlaneNdc = 1.0f;
    const glm::vec3 click_pos =
        TransformPoint(ndc_to_object, glm::vec3{click_ndc, kFarPlaneNdc});
    return Ray{.start = camera_pos, .direction = click_pos - camera_pos};
  } else if constexpr (std::is_same_v<CameraType, OrthographicCamera>) {
    #ifdef USE_VULKAN
    constexpr float kNearPlaneNdc = 0.0f;
#else  // !USE_VULKAN
    constexpr float kNearPlaneNdc = -1.0f;
#endif  // USE_VULKAN
    const glm::vec3 click_pos =
        TransformPoint(ndc_to_object, glm::vec3{click_ndc, kNearPlaneNdc});
    const glm::vec3 camera_dir =
        TransformVector(world_to_object, camera.front());
    return Ray{.start = click_pos, .direction = camera_dir};
  } else {
    static_assert("Unhandled camera type");
  }
}

template <typename CameraType>
std::optional<glm::vec3> CameraViewedSphere<CameraType>::GetIntersection(
    const CameraType& camera, const glm::vec2& click_ndc) const {
  const Ray ray = GetClickingRay(camera, click_ndc);
  glm::vec3 position, normal;
  if (glm::intersectRaySphere(
          ray.start, glm::normalize(ray.direction),
          /*sphereCenter=*/glm::vec3{0.0f}, /*sphereRadius=*/1.0f,
          position, normal)) {
    return position;
  } else {
    return std::nullopt;
  }
}

template <typename CameraType>
std::optional<rotation::Rotation> CameraViewedSphere<CameraType>::ShouldRotate(
    const CameraType& camera, const std::optional<glm::vec2>& click_ndc) {
  const auto intersection = click_ndc.has_value()
                                ? GetIntersection(camera, click_ndc.value())
                                : std::nullopt;
  return Sphere::ShouldRotate(intersection);
}

template class CameraViewedSphere<PerspectiveCamera>;
template class CameraViewedSphere<OrthographicCamera>;

}  // namespace lighter::common
