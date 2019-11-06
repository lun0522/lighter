//
//  camera.cc
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/camera.h"

#include "jessie_steamer/common/util.h"
#include "third_party/glm/gtc/matrix_transform.hpp"
#include "third_party/glm/gtx/vector_angle.hpp"

namespace jessie_steamer {
namespace common {
namespace {

// Returns the reference front vector in zx-plane.
const glm::vec2& GetRefFrontInZxPlane() {
  static const glm::vec2 ref_front_zx{1.0f, 0.0f};
  return ref_front_zx;
}

// Returns the front vector given current position and look at point.
inline glm::vec3 ComputeFront(const glm::vec3& current_pos,
                              const glm::vec3& look_at) {
  return glm::normalize(look_at - current_pos);
}

} /* namespace */

Camera::Camera(const Config& config)
    : near_{config.near}, far_{config.far}, up_{config.up},
      fov_{config.field_of_view},
      fov_aspect_ratio_{config.fov_aspect_ratio}, pos_{config.position} {
  UpdateDirection(ComputeFront(pos_, config.look_at));
}

void Camera::UpdateFieldOfView(float fov) {
  fov_ = fov;
  UpdateProjection();
}

void Camera::UpdateFovAspectRatio(float aspect_ratio) {
  fov_aspect_ratio_ = aspect_ratio;
  UpdateProjection();
}

void Camera::UpdateProjection() {
  proj_ = glm::perspective(glm::radians(fov_), fov_aspect_ratio_, near_, far_);
}

void Camera::UpdatePosition(const glm::vec3& offset) {
  pos_ += offset;
  UpdateView();
}

void Camera::UpdateDirection(const glm::vec3& front) {
  front_ = front;
  right_ = glm::normalize(glm::cross(front_, up_));
  UpdateView();
}

void Camera::UpdateView() {
  view_ = glm::lookAt(pos_, pos_ + front_, up_);
}

UserControlledCamera::UserControlledCamera(const Config& config,
                                           const ControlConfig& control_config,
                                           float fov_aspect_ratio)
    : Camera{config},
      move_speed_{control_config.move_speed},
      turn_speed_{control_config.turn_speed},
      center_{config.look_at}, lock_center_{control_config.lock_center},
      pitch_{glm::asin(front().y)},
      yaw_{glm::orientedAngle(
          GetRefFrontInZxPlane(),
          glm::normalize(glm::vec2{front().z, front().x}))} {
  UpdateFovAspectRatio(fov_aspect_ratio);
}

void UserControlledCamera::DidMoveCursor(double x, double y) {
  if (!is_active_ || lock_center_) {
    return;
  }

  const auto offset_x = static_cast<float>((x - cursor_pos_.x) * turn_speed_);
  const auto offset_y = static_cast<float>((y - cursor_pos_.y) * turn_speed_);
  cursor_pos_ = glm::dvec2{x, y};
  pitch_ = glm::clamp(pitch_ - offset_y, glm::radians(-89.9f),
                      glm::radians(89.9f));
  yaw_ = glm::mod(yaw_ - offset_x, glm::radians(360.0f));
  UpdateDirection(/*front=*/{
      glm::cos(pitch_) * glm::sin(yaw_),
      glm::sin(pitch_),
      glm::cos(pitch_) * glm::cos(yaw_),
  });
}

void UserControlledCamera::DidScroll(
    double delta, double min_val, double max_val) {
  if (!is_active_) {
    return;
  }

  UpdateFieldOfView(static_cast<float>(
      glm::clamp(field_of_view() + delta, min_val, max_val)));
}

void UserControlledCamera::DidPressKey(ControlKey key, float elapsed_time) {
  if (!is_active_) {
    return;
  }

  const float distance = elapsed_time * move_speed_;
  switch (key) {
    case ControlKey::kUp:
      UpdatePosition(/*offset=*/+front() * distance);
      break;
    case ControlKey::kDown:
      UpdatePosition(/*offset=*/-front() * distance);
      break;
    case ControlKey::kLeft:
      UpdatePosition(/*offset=*/-right() * distance);
      break;
    case ControlKey::kRight:
      UpdatePosition(/*offset=*/+right() * distance);
      break;
  }

  if (lock_center_) {
    UpdateDirection(ComputeFront(position(), center_));
  }
}

} /* namespace common */
} /* namespace jessie_steamer */
