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
    : near_{config.near}, far_{config.far},
      up_{config.up}, pos_{config.position} {
  UpdateDirection(ComputeFront(pos_, config.look_at));
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

glm::mat4 Camera::GetSkyboxViewMatrix() const {
  return glm::mat4{glm::mat3{view_}};
}

PerspectiveCamera::PerspectiveCamera(const Camera::Config& config,
                                     const PersConfig& pers_config)
    : Camera{config}, fov_{pers_config.field_of_view},
      fov_aspect_ratio_{pers_config.fov_aspect_ratio} {
  UpdateProjection();
}

void PerspectiveCamera::UpdateFieldOfView(float fov) {
  fov_ = fov;
  UpdateProjection();
}

void PerspectiveCamera::UpdateProjection() {
  proj_ = glm::perspective(glm::radians(fov_), fov_aspect_ratio_, near_, far_);
}

OrthographicCamera::OrthographicCamera(const Camera::Config& config,
                                       const OrthoConfig& ortho_config)
    : Camera{config}, aspect_ratio_{ortho_config.aspect_ratio},
      view_width_{ortho_config.view_width} {
  UpdateProjection();
}

void OrthographicCamera::UpdateViewWidth(float view_width) {
  view_width_ = view_width;
  UpdateProjection();
}

void OrthographicCamera::UpdateProjection() {
  const float view_height = view_width_ / aspect_ratio_;
  const auto half_view_size = glm::vec2{view_width_, view_height} / 2.0f;
  proj_ = glm::ortho(-half_view_size.x, half_view_size.x,
                     -half_view_size.y, half_view_size.y, near_, far_);
}

UserControlledCamera::UserControlledCamera(const ControlConfig& control_config,
                                           std::unique_ptr<Camera>&& camera)
    : camera_{std::move(camera)},
      move_speed_{control_config.move_speed},
      turn_speed_{control_config.turn_speed},
      lock_center_{control_config.lock_center} {
  const glm::vec3& front = camera_->front();
  pitch_ = glm::asin(front.y);
  yaw_ = glm::orientedAngle(
      GetRefFrontInZxPlane(), glm::normalize(glm::vec2{front.z, front.x}));
  UpdateDirectionIfNeeded();
}

void UserControlledCamera::DidMoveCursor(double x, double y) {
  if (!is_active_ || lock_center_.has_value()) {
    return;
  }

  const auto offset_x = static_cast<float>((x - cursor_pos_.x) * turn_speed_);
  const auto offset_y = static_cast<float>((y - cursor_pos_.y) * turn_speed_);
  cursor_pos_ = glm::dvec2{x, y};
  pitch_ = glm::clamp(pitch_ - offset_y, glm::radians(-89.9f),
                      glm::radians(89.9f));
  yaw_ = glm::mod(yaw_ - offset_x, glm::radians(360.0f));
  camera_->UpdateDirection(/*front=*/{
      glm::cos(pitch_) * glm::sin(yaw_),
      glm::sin(pitch_),
      glm::cos(pitch_) * glm::cos(yaw_),
  });
}

bool UserControlledCamera::DidScroll(
    double delta, double min_val, double max_val) {
  if (!is_active_) {
    return false;
  }

  if (auto* pers_camera = dynamic_cast<PerspectiveCamera*>(camera_.get())) {
    const float new_fov =
        glm::clamp(pers_camera->field_of_view() + delta, min_val, max_val);
    if (new_fov != pers_camera->field_of_view()) {
      pers_camera->UpdateFieldOfView(new_fov);
      return true;
    }
    return false;
  }

  if (auto* ortho_camera = dynamic_cast<OrthographicCamera*>(camera_.get())) {
    const float new_width =
        glm::clamp(ortho_camera->view_width() + delta, min_val, max_val);
    if (new_width != ortho_camera->view_width()) {
      ortho_camera->UpdateViewWidth(new_width);
      return true;
    }
    return false;
  }

  FATAL("Unrecognized camera type");
}

void UserControlledCamera::DidPressKey(ControlKey key, float elapsed_time) {
  if (!is_active_) {
    return;
  }

  const float distance = elapsed_time * move_speed_;
  switch (key) {
    case ControlKey::kUp:
      camera_->UpdatePosition(/*offset=*/+camera_->front() * distance);
      break;
    case ControlKey::kDown:
      camera_->UpdatePosition(/*offset=*/-camera_->front() * distance);
      break;
    case ControlKey::kLeft:
      camera_->UpdatePosition(/*offset=*/-camera_->right() * distance);
      break;
    case ControlKey::kRight:
      camera_->UpdatePosition(/*offset=*/+camera_->right() * distance);
      break;
  }
  UpdateDirectionIfNeeded();
}

void UserControlledCamera::UpdateDirectionIfNeeded() {
  if (lock_center_.has_value()) {
    camera_->UpdateDirection(
        ComputeFront(camera_->position(), lock_center_.value()));
  }
}

} /* namespace common */
} /* namespace jessie_steamer */
