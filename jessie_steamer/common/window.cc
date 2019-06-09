//
//  window.cc
//
//  Created by Pujun Lun on 3/27/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/window.h"

namespace jessie_steamer {
namespace common {
namespace {

using std::runtime_error;

std::function<void()> set_resized_flag = nullptr;
Window::CursorMoveCallback cursor_move_callback = nullptr;
Window::ScrollCallback scroll_callback = nullptr;

void DidResizeWindow(GLFWwindow* window, int width, int height) {
  if (set_resized_flag != nullptr) {
    set_resized_flag();
  }
}

void DidMoveCursor(GLFWwindow* window, double x_pos, double y_pos) {
  if (cursor_move_callback != nullptr) {
    cursor_move_callback(x_pos, y_pos);
  }
}

void DidScroll(GLFWwindow* window, double x_pos, double y_pos) {
  if (scroll_callback != nullptr) {
    scroll_callback(x_pos, y_pos);
  }
}

int WindowKeyToGlfwKey(Window::KeyMap key) {
  using KeyMap = Window::KeyMap;
  switch (key) {
    case KeyMap::kEscape:
      return GLFW_KEY_ESCAPE;
    case KeyMap::kUp:
      return GLFW_KEY_UP;
    case KeyMap::kDown:
      return GLFW_KEY_DOWN;
    case KeyMap::kLeft:
      return GLFW_KEY_LEFT;
    case KeyMap::kRight:
      return GLFW_KEY_RIGHT;
  }
}

} /* namespace */

void Window::Init(const std::string& name, glm::ivec2 screen_size) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  if (glfwVulkanSupported() == GL_FALSE) {
    throw runtime_error{"Vulkan not supported"};
  }

  window_ = glfwCreateWindow(screen_size.x, screen_size.y, name.c_str(),
                             nullptr, nullptr);
  if (window_ == nullptr) {
    throw runtime_error{"Failed to create window"};
  }
  glfwMakeContextCurrent(window_);

  set_resized_flag = [this]() { is_resized_ = true; };
  glfwSetFramebufferSizeCallback(window_, DidResizeWindow);
  glfwSetCursorPosCallback(window_, DidMoveCursor);
  glfwSetScrollCallback(window_, DidScroll);
}

#ifdef USE_VULKAN
VkSurfaceKHR Window::CreateSurface(const VkInstance& instance,
                                       const VkAllocationCallbacks* allocator) {
  VkSurfaceKHR surface;
  auto result = glfwCreateWindowSurface(instance, window_, allocator, &surface);
  if (result != VK_SUCCESS) {
    throw runtime_error{"Failed to create window surface"};
  }
  return surface;
}
#endif /* USE_VULKAN */

void Window::SetCursorHidden(bool hidden) {
  glfwSetInputMode(window_, GLFW_CURSOR,
                   hidden ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void Window::RegisterKeyCallback(KeyMap key, const KeyCallback& callback) {
  const auto glfw_key = WindowKeyToGlfwKey(key);
  if (callback != nullptr) {
    key_callbacks_.emplace(glfw_key, callback);
  } else {
    key_callbacks_.erase(glfw_key);
  }
}

void Window::RegisterCursorMoveCallback(CursorMoveCallback callback) {
  cursor_move_callback = std::move(callback);
}

void Window::RegisterScrollCallback(ScrollCallback callback) {
  scroll_callback = std::move(callback);
}

void Window::PollEvents() {
  glfwPollEvents();
  for (const auto& pair : key_callbacks_) {
    if (glfwGetKey(window_, pair.first) == GLFW_PRESS) {
      pair.second();
    }
  }
}

#ifdef USE_VULKAN
const std::vector<const char*>& Window::required_extensions() {
  static std::vector<const char*>* kRequiredExtensions = nullptr;
  if (kRequiredExtensions == nullptr) {
    uint32_t extension_count;
    const char** glfw_extensions =
        glfwGetRequiredInstanceExtensions(&extension_count);
    kRequiredExtensions = new std::vector<const char*>{
        glfw_extensions, glfw_extensions + extension_count};
  }
  return *kRequiredExtensions;
}
#endif /* USE_VULKAN */

glm::ivec2 Window::GetScreenSize() const {
  glm::ivec2 extent;
  glfwGetFramebufferSize(window_, &extent.x, &extent.y);
  return extent;
}

glm::dvec2 Window::GetCursorPos() const {
  glm::dvec2 pos;
  glfwGetCursorPos(window_, &pos.x, &pos.y);
  return pos;
}

bool Window::IsMinimized() const {
  auto extent = GetScreenSize();
  return extent.x == 0 || extent.y == 0;
}

Window::~Window() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

} /* namespace common */
} /* namespace jessie_steamer */
