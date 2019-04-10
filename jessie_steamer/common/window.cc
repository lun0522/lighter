//
//  window.cc
//
//  Created by Pujun Lun on 3/27/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "window.h"

#include <stdexcept>

#include "jessie_steamer/common/util.h"

namespace jessie_steamer {
namespace common {
namespace window {
namespace glfw_window {

using SetResizedFlag = std::function<void()>;

SetResizedFlag set_resized_flag = nullptr;
CursorMoveCallback cursor_move_callback = nullptr;
ScrollCallback scroll_callback = nullptr;

void DidResizeWindow(GLFWwindow* window, int width, int height) {
  set_resized_flag();
}

void DidMoveCursor(GLFWwindow* window, double x_pos, double y_pos) {
  if (cursor_move_callback) {
    cursor_move_callback(x_pos, y_pos);
  }
}

void DidScroll(GLFWwindow *window, double x_pos, double y_pos) {
  if (scroll_callback) {
    scroll_callback(x_pos, y_pos);
  }
}

} /* namespace glfw_window */

void GlfwWindow::Init(const std::string& name, glm::ivec2 screen_size) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  window_ = glfwCreateWindow(screen_size.x, screen_size.y, name.c_str(),
                             nullptr, nullptr);
  if (window_ == nullptr) {
    throw std::runtime_error{"Failed to create window"};
  }
  glfwMakeContextCurrent(window_);

  glfw_window::set_resized_flag = [this]() { Window::is_resized_ = true; };
  glfwSetFramebufferSizeCallback(window_, glfw_window::DidResizeWindow);
  glfwSetCursorPosCallback(window_, glfw_window::DidMoveCursor);
  glfwSetScrollCallback(window_, glfw_window::DidScroll);
}

VkSurfaceKHR GlfwWindow::CreateSurface(const VkInstance& instance,
                                       const VkAllocationCallbacks* allocator) {
  VkSurfaceKHR surface;
  ASSERT_SUCCESS(glfwCreateWindowSurface(instance, window_, allocator, &surface),
                 "Failed to create window surface");
  return surface;
}

void GlfwWindow::SetCursorHidden(bool hidden) {
  glfwSetInputMode(window_, GLFW_CURSOR,
                   hidden ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void GlfwWindow::RegisterKeyCallback(key_map::KeyMap key, KeyCallback callback) {
  int glfw_key;
  switch (key) {
    case key_map::KeyMap::kEscape:
      glfw_key = GLFW_KEY_ESCAPE;
      break;
    case key_map::KeyMap::kUp:
      glfw_key = GLFW_KEY_UP;
      break;
    case key_map::KeyMap::kDown:
      glfw_key = GLFW_KEY_DOWN;
      break;
    case key_map::KeyMap::kLeft:
      glfw_key = GLFW_KEY_LEFT;
      break;
    case key_map::KeyMap::kRight:
      glfw_key = GLFW_KEY_RIGHT;
      break;
  }

  if (callback) {
    key_callbacks_.emplace(glfw_key, std::move(callback));
  } else {
    key_callbacks_.erase(glfw_key);
  }
}

void GlfwWindow::RegisterCursorMoveCallback(CursorMoveCallback callback) {
  glfw_window::cursor_move_callback = std::move(callback);
}

void GlfwWindow::RegisterScrollCallback(ScrollCallback callback) {
  glfw_window::scroll_callback = std::move(callback);
}

void GlfwWindow::PollEvents() {
  glfwPollEvents();
  for (const auto& pair : key_callbacks_) {
    if (glfwGetKey(window_, pair.first) == GLFW_PRESS) {
      pair.second();
    }
  }
}

bool GlfwWindow::IsMinimized() const {
  auto extent = screen_size();
  return extent.x == 0 || extent.y == 0;
}

glm::ivec2 GlfwWindow::screen_size() const {
  glm::ivec2 extent;
  glfwGetFramebufferSize(window_, &extent.x, &extent.y);
  return extent;
}

glm::dvec2 GlfwWindow::cursor_pos() const {
  glm::dvec2 pos;
  glfwGetCursorPos(window_, &pos.x, &pos.y);
  return pos;
}

GlfwWindow::~GlfwWindow() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

} /* namespace window */
} /* namespace common */
} /* namespace jessie_steamer */
