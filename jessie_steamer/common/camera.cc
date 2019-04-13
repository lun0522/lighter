//
//  camera.cc
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/camera.h"

#include <stdexcept>

#include "third_party/glm/gtc/matrix_transform.hpp"

namespace jessie_steamer {
namespace common {
namespace {

using glm::radians;
using glm::vec3;

} /* namespace */

Camera::Camera(const vec3& position, const vec3& front, const vec3& up,
               float fov, float near, float far,
               float yaw, float pitch, float sensitivity)
    : fov_{fov}, near_{near}, far_{far},
      yaw_{yaw}, pitch_{pitch}, sensitivity_{sensitivity},
      pos_{position}, front_{front}, up_{up} {
  UpdateRightVector();
  UpdateViewMatrix();
}

void Camera::UpdateFrontVector() {
  front_ = vec3(glm::cos(radians(pitch_)) * glm::cos(radians(yaw_)),
                glm::sin(radians(pitch_)),
                glm::cos(radians(pitch_)) * glm::sin(radians(yaw_)));
}

void Camera::UpdateRightVector() {
  right_ = glm::normalize(glm::cross(front_, up_));
}

void Camera::UpdateViewMatrix() {
  view_ = glm::lookAt(pos_, pos_ + front_, up_);
}

void Camera::UpdateProjMatrix() {
  proj_ = glm::perspective(radians(fov_),
                           (float)screen_size_.x / screen_size_.y, near_, far_);
}

void Camera::Init(const glm::ivec2& screen_size, const glm::dvec2& cursor_pos) {
  screen_size_ = screen_size;
  cursor_pos_ = cursor_pos;
  UpdateProjMatrix();
}

void Camera::ProcessCursorMove(double x, double y) {
  float x_offset = (x - cursor_pos_.x) * sensitivity_;
  float y_offset = (cursor_pos_.y - y) * sensitivity_;
  cursor_pos_ = glm::dvec2{x, y};
  yaw_ = glm::mod(yaw_ + x_offset, 360.0f);
  pitch_ = glm::clamp(pitch_ + y_offset, -89.0f, 89.0f);

  UpdateFrontVector();
  UpdateRightVector();
  UpdateViewMatrix();
}

void Camera::ProcessScroll(double y, double min_val, double max_val) {
  fov_ = glm::clamp(fov_ + y, min_val, max_val);
  UpdateProjMatrix();
}

void Camera::ProcessKey(Window::KeyMap key, float elapsed_time) {
  using KeyMap = Window::KeyMap;
  float distance = elapsed_time * 5.0f;
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
  UpdateViewMatrix();
}

} /* namespace common */
} /* namespace jessie_steamer */
