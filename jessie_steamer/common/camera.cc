//
//  camera.cc
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/camera.h"

#include <stdexcept>

#include "third_party/glm/gtc/matrix_transform.hpp"
#include "third_party/glm/gtx/vector_angle.hpp"

namespace jessie_steamer {
namespace common {
namespace {

using glm::radians;
using glm::vec3;

const glm::vec2& ref_front_zx() {
  static const glm::vec2 kRefFrontZx{1.0f, 0.0f};
  return kRefFrontZx;
}

} /* namespace */

void Camera::Init(const Config& config) {
  up_ = config.up;
  pos_ = config.pos;
  front_ = glm::normalize(config.look_at - config.pos);
  fov_ = config.fov;
  near_ = config.near;
  far_ = config.far;
  const glm::vec2 front_zx = glm::normalize(glm::vec2{front_.z, front_.x});
  yaw_ = glm::orientedAngle(ref_front_zx(), front_zx);
  pitch_ = glm::asin(front_.y);
  move_speed_ = config.move_speed;
  turn_speed_ = config.turn_speed;

  UpdateRight();
  UpdateView();
}

void Camera::Calibrate(const glm::ivec2& screen_size,
                       const glm::dvec2& cursor_pos) {
  screen_size_ = screen_size;
  cursor_pos_ = cursor_pos;
  UpdateProj();
}

void Camera::Activate() {
  is_active_ = true;
}

void Camera::Deactivate() {
  is_active_ = false;
}

void Camera::ProcessCursorMove(double x, double y) {
  if (!is_active_) {
    return;
  }

  const auto x_offset = static_cast<float>((x - cursor_pos_.x) * turn_speed_);
  const auto y_offset = static_cast<float>((y - cursor_pos_.y) * turn_speed_);
  cursor_pos_ = glm::dvec2{x, y};
  yaw_ = glm::mod(yaw_ - x_offset, radians(360.0f));
  pitch_ = glm::clamp(pitch_ - y_offset, radians(-89.9f), radians(89.9f));
  UpdateFront();
  UpdateRight();
  UpdateView();
}

void Camera::ProcessScroll(double y, double min_val, double max_val) {
  if (!is_active_) {
    return;
  }

  fov_ = static_cast<float>(glm::clamp(fov_ + y, min_val, max_val));
  UpdateProj();
}

void Camera::ProcessKey(Window::KeyMap key, float elapsed_time) {
  if (!is_active_) {
    return;
  }

  using KeyMap = Window::KeyMap;
  const float distance = elapsed_time * move_speed_;
  switch (key) {
    case KeyMap::kUp:
      pos_ += front_ * distance;
      break;
    case KeyMap::kDown:
      pos_ -= front_ * distance;
      break;
    case KeyMap::kLeft:
      pos_ -= right_ * distance;
      break;
    case KeyMap::kRight:
      pos_ += right_ * distance;
      break;
    default:
      throw std::runtime_error{"Unsupported key"};
  }
  UpdateView();
}

void Camera::UpdateFront() {
  front_ = vec3(glm::cos(pitch_) * glm::sin(yaw_),
                glm::sin(pitch_),
                glm::cos(pitch_) * glm::cos(yaw_));
}

void Camera::UpdateRight() {
  right_ = glm::normalize(glm::cross(front_, up_));
}

void Camera::UpdateView() {
  view_ = glm::lookAt(pos_, pos_ + front_, up_);
}

void Camera::UpdateProj() {
  proj_ = glm::perspective(radians(fov_),
                           (float)screen_size_.x / screen_size_.y, near_, far_);
}

} /* namespace common */
} /* namespace jessie_steamer */
