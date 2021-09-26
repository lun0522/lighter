//
//  camera.cc
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "lighter/common/camera.h"

#include <type_traits>

#include "lighter/common/util.h"
#include "third_party/glm/gtc/matrix_transform.hpp"

namespace lighter::common {

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

glm::mat4 Camera::GetViewMatrix() const {
  return glm::lookAt(pos_, pos_ + front_, up_);
}

PerspectiveCamera& PerspectiveCamera::SetFieldOfViewY(float fovy) {
  fovy_ = fovy;
  return *this;
}

PerspectiveCamera::RayTracingParams
PerspectiveCamera::GetRayTracingParams() const {
  const glm::vec3 up_dir = glm::normalize(glm::cross(right(), front()));
  const float tan_fovy = glm::tan(glm::radians(fovy_));
  return RayTracingParams{
      .up = up_dir * tan_fovy,
      .front = front(),
      .right = right() * tan_fovy * aspect_ratio_,
  };
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

template <typename CameraType>
void UserControlledCamera<CameraType>::SetInternalStates(
    absl::FunctionRef<void(CameraType*)> operation) {
  operation(camera_.get());
  ResetAngles();
}

template <typename CameraType>
void UserControlledCamera<CameraType>::DidMoveCursor(double x, double y) {
  if (!is_active_) {
    return;
  }

  const auto offset_x = static_cast<float>((x - cursor_pos_.x) *
                        control_config_.turn_speed);
  const auto offset_y = static_cast<float>((y - cursor_pos_.y) *
                        control_config_.turn_speed);
  cursor_pos_ = glm::dvec2{x, y};
  pitch_ = glm::clamp(pitch_ - offset_y, glm::radians(-89.9f),
                      glm::radians(89.9f));
  yaw_ = glm::mod(yaw_ - offset_x, glm::radians(360.0f));
  camera_->SetFront({glm::cos(pitch_) * glm::sin(yaw_) * ref_left_ +
                     glm::cos(pitch_) * glm::cos(yaw_) * ref_front_ +
                     glm::sin(pitch_) * camera_->up()});
}

template <typename CameraType>
bool UserControlledCamera<CameraType>::DidScroll(
    double delta, double min_val, double max_val) {
  if (!is_active_) {
    return false;
  }

  if constexpr (std::is_same_v<CameraType, PerspectiveCamera>) {
    const float new_fovy = glm::clamp(camera_->field_of_view_y() + delta,
                                      min_val, max_val);
    if (new_fovy != camera_->field_of_view_y()) {
      camera_->SetFieldOfViewY(new_fovy);
      return true;
    }
  } else if constexpr (std::is_same_v<CameraType, OrthographicCamera>) {
    const float new_width = glm::clamp(camera_->view_width() + delta,
                                      min_val, max_val);
    if (new_width != camera_->view_width()) {
      camera_->SetViewWidth(new_width);
      return true;
    }
  } else {
    static_assert("Unhandled camera type");
  }
  return false;
}

template <typename CameraType>
void UserControlledCamera<CameraType>::DidPressKey(camera_control::Key key,
                                                   float elapsed_time) {
  using ControlKey = camera_control::Key;

  if (!is_active_) {
    return;
  }

  if (control_config_.lock_center.has_value()) {
    const glm::vec3& front = camera_->front();
    const glm::vec3& center = control_config_.lock_center.value();
    const glm::vec3 pos_to_center = center - camera_->position();
    const glm::vec3 normalized_pos_to_center = glm::normalize(pos_to_center);

    float angle_sign;
    glm::vec3 rotation_axis;
    switch (key) {
      case ControlKey::kUp:
      case ControlKey::kDown: {
        if (1.0f - glm::abs(glm::dot(normalized_pos_to_center, front)) < 1E-5) {
          return;
        }
        angle_sign = key == ControlKey::kUp ? 1.0f : -1.0f;
        rotation_axis = glm::normalize(glm::cross(front, pos_to_center));
        break;
      }

      case ControlKey::kLeft:
      case ControlKey::kRight: {
        const glm::vec3& right = camera_->right();
        if (1.0f - glm::abs(glm::dot(normalized_pos_to_center, right)) < 1E-5) {
          return;
        }
        angle_sign = key == ControlKey::kRight ? 1.0f : -1.0f;
        rotation_axis = glm::normalize(glm::cross(right, pos_to_center));
        break;
      }
    }

    const float angle = elapsed_time * control_config_.move_speed * angle_sign;
    const glm::mat4 rotation =
        glm::rotate(glm::mat4{1.0f}, angle, rotation_axis);
    const glm::vec3 new_pos_to_center =
        rotation * glm::vec4{pos_to_center, 0.0f};
    const glm::vec3 new_front = rotation * glm::vec4{front, 0.0f};
    camera_->SetPosition(center - new_pos_to_center);
    camera_->SetFront(new_front);
    ResetAngles();

  } else {
    const float distance = elapsed_time * control_config_.move_speed;
    switch (key) {
      case ControlKey::kUp:
        camera_->UpdatePositionByOffset(+camera_->front() * distance);
        break;
      case ControlKey::kDown:
        camera_->UpdatePositionByOffset(-camera_->front() * distance);
        break;
      case ControlKey::kLeft:
        camera_->UpdatePositionByOffset(-camera_->right() * distance);
        break;
      case ControlKey::kRight:
        camera_->UpdatePositionByOffset(+camera_->right() * distance);
        break;
    }
  }
}

template <typename CameraType>
void UserControlledCamera<CameraType>::ResetAngles() {
  ref_front_ = camera_->front();
  ref_left_ = -camera_->right();
  pitch_ = yaw_ = 0.0f;
}

template class UserControlledCamera<PerspectiveCamera>;
template class UserControlledCamera<OrthographicCamera>;

}  // namespace lighter::common
