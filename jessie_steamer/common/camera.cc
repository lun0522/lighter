//
//  camera.cc
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/camera.h"

#include "jessie_steamer/common/util.h"
#include "third_party/glm/gtc/matrix_transform.hpp"

namespace jessie_steamer {
namespace common {

Camera& Camera::UpdatePositionByOffset(const glm::vec3& offset) {
  pos_ += offset;
  return *this;
}

Camera& Camera::SetPosition(const glm::vec3& position) {
  pos_ = position;
  return *this;
}

// Updates the up vector and view matrix. 'up' does not need to be normalized.
Camera& Camera::SetUp(const glm::vec3& up) {
  up_ = glm::normalize(up);
  return *this;
}

Camera& Camera::SetFront(const glm::vec3& front) {
  front_ = glm::normalize(front);
  right_ = glm::normalize(glm::cross(front_, up_));
  return *this;
}

Camera& Camera:: SetRight(const glm::vec3& right) {
  right_ = glm::normalize(right);
  front_ = glm::normalize(glm::cross(up_, right_));
  return *this;
}

glm::mat4 Camera::GetViewMatrix() const {
  return glm::lookAt(pos_, pos_ + front_, up_);
}

PerspectiveCamera& PerspectiveCamera::SetFieldOfViewY(float fovy) {
  fovy_ = fovy;
  return *this;
}

glm::mat4 PerspectiveCamera::GetProjectionMatrix() const {
  return glm::perspective(glm::radians(fovy_), aspect_ratio_, near_, far_);
}

OrthographicCamera& OrthographicCamera::SetViewWidth(float view_width) {
  view_width_ = view_width;
  return *this;
}

glm::mat4 OrthographicCamera::GetProjectionMatrix() const {
  const float view_height = view_width_ / aspect_ratio_;
  const auto half_view_size = glm::vec2{view_width_, view_height} / 2.0f;
  return glm::ortho(-half_view_size.x, half_view_size.x,
                    -half_view_size.y, half_view_size.y, near_, far_);
}

void UserControlledCamera::SetInternalStates(
    const std::function<void(Camera*)>& operation) {
  operation(camera_.get());
  Reset();
}

void UserControlledCamera::DidMoveCursor(double x, double y) {
  if (!is_active_) {
    return;
  }

  const auto offset_x = static_cast<float>((x - cursor_pos_.x) * turn_speed_);
  const auto offset_y = static_cast<float>((y - cursor_pos_.y) * turn_speed_);
  cursor_pos_ = glm::dvec2{x, y};
  pitch_ = glm::clamp(pitch_ - offset_y, glm::radians(-89.9f),
                      glm::radians(89.9f));
  yaw_ = glm::mod(yaw_ - offset_x, glm::radians(360.0f));
  camera_->SetFront({glm::cos(pitch_) * glm::sin(yaw_) * ref_left_ +
                     glm::cos(pitch_) * glm::cos(yaw_) * ref_front_ +
                     glm::sin(pitch_) * camera_->up()});
}

bool UserControlledCamera::DidScroll(
    double delta, double min_val, double max_val) {
  if (!is_active_) {
    return false;
  }

  if (auto* pers_camera = dynamic_cast<PerspectiveCamera*>(camera_.get())) {
    const float new_fovy =
        glm::clamp(pers_camera->field_of_view_y() + delta, min_val, max_val);
    if (new_fovy != pers_camera->field_of_view_y()) {
      pers_camera->SetFieldOfViewY(new_fovy);
      return true;
    }
    return false;
  }

  if (auto* ortho_camera = dynamic_cast<OrthographicCamera*>(camera_.get())) {
    const float new_width =
        glm::clamp(ortho_camera->view_width() + delta, min_val, max_val);
    if (new_width != ortho_camera->view_width()) {
      ortho_camera->SetViewWidth(new_width);
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
      camera_->UpdatePositionByOffset(/*offset=*/+camera_->front() * distance);
      break;
    case ControlKey::kDown:
      camera_->UpdatePositionByOffset(/*offset=*/-camera_->front() * distance);
      break;
    case ControlKey::kLeft:
      camera_->UpdatePositionByOffset(/*offset=*/-camera_->right() * distance);
      break;
    case ControlKey::kRight:
      camera_->UpdatePositionByOffset(/*offset=*/+camera_->right() * distance);
      break;
  }
}

void UserControlledCamera::Reset() {
  ref_front_ = camera_->front();
  ref_left_ = -camera_->right();
  pitch_ = yaw_ = 0.0f;
}

} /* namespace common */
} /* namespace jessie_steamer */
