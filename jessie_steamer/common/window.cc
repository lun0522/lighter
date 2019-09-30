//
//  window.cc
//
//  Created by Pujun Lun on 3/27/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/window.h"

#include "jessie_steamer/common/util.h"
#include "third_party/glfw/glfw3.h"

namespace jessie_steamer {
namespace common {
namespace {

using std::vector;

// Translates the key we defined to its counterpart in GLFW.
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

namespace window_callback {

// We pass a pointer to the Window instance with 'glfwSetWindowUserPointer'
// and retrieve it with 'glfwGetWindowUserPointer', so that we can relay
// function calls to the holder of callbacks.
Window& GetWindow(GLFWwindow* window) {
  return *static_cast<Window*>(glfwGetWindowUserPointer(window));
}
void GlfwResizeWindowCallback(GLFWwindow* window, int width, int height) {
  GetWindow(window).DidResizeWindow();
}
void GlfwMoveCursorCallback(GLFWwindow* window, double x_pos, double y_pos) {
  GetWindow(window).DidMoveCursor(x_pos, y_pos);
}
void GlfwScrollCallback(GLFWwindow* window, double x_pos, double y_pos) {
  GetWindow(window).DidScroll(x_pos, y_pos);
}

} /* namespace window_callback */

const vector<const char*>& Window::GetRequiredExtensions() {
  static vector<const char*>* required_extensions = nullptr;
  if (required_extensions == nullptr) {
    uint32_t extension_count;
    const char** glfw_extensions =
        glfwGetRequiredInstanceExtensions(&extension_count);
    required_extensions = new vector<const char*>{
        glfw_extensions, glfw_extensions + extension_count};
  }
  return *required_extensions;
}

Window::Window(const std::string& name, const glm::ivec2& screen_size) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
  ASSERT_TRUE(glfwVulkanSupported() == GLFW_TRUE, "Vulkan is not supported");

  window_ = glfwCreateWindow(screen_size.x, screen_size.y, name.c_str(),
                             /*monitor=*/nullptr, /*share=*/nullptr);
  ASSERT_NON_NULL(window_, "Failed to create window");

  glfwMakeContextCurrent(window_);
  glfwSetWindowUserPointer(window_, this);
  glfwSetFramebufferSizeCallback(
      window_, window_callback::GlfwResizeWindowCallback);
  glfwSetCursorPosCallback(window_, window_callback::GlfwMoveCursorCallback);
  glfwSetScrollCallback(window_, window_callback::GlfwScrollCallback);
}

#ifdef USE_VULKAN
VkSurfaceKHR Window::CreateSurface(const VkInstance& instance,
                                   const VkAllocationCallbacks* allocator) {
  VkSurfaceKHR surface;
  auto result = glfwCreateWindowSurface(instance, window_, allocator, &surface);
  ASSERT_TRUE(result == VK_SUCCESS, "Failed to create window surface");
  return surface;
}
#endif /* USE_VULKAN */

Window& Window::SetCursorHidden(bool hidden) {
  glfwSetInputMode(window_, GLFW_CURSOR,
                   hidden ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
  return *this;
}

Window& Window::RegisterPressKeyCallback(
    KeyMap key, PressKeyCallback&& callback) {
  const auto glfw_key = WindowKeyToGlfwKey(key);
  if (callback == nullptr) {
    press_key_callbacks_.erase(glfw_key);
  } else {
    press_key_callbacks_.emplace(glfw_key, std::move(callback));
  }
  return *this;
}

Window& Window::RegisterMoveCursorCallback(MoveCursorCallback&& callback) {
  move_cursor_callback_ = std::move(callback);
  return *this;
}

Window& Window::RegisterScrollCallback(ScrollCallback&& callback) {
  scroll_callback_ = std::move(callback);
  return *this;
}

void Window::ProcessUserInputs() const {
  glfwPollEvents();
  for (const auto& callback : press_key_callbacks_) {
    if (glfwGetKey(window_, callback.first) == GLFW_PRESS) {
      callback.second();
    }
  }
}

glm::ivec2 Window::Recreate() {
  glm::ivec2 extent{};
  while (extent.x == 0 || extent.y == 0) {
    glfwWaitEvents();
    extent = GetScreenSize();
  }
  is_resized_ = false;
  return extent;
}

bool Window::ShouldQuit() const {
  return glfwWindowShouldClose(window_);
}

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

void Window::DidResizeWindow() {
  is_resized_ = true;
}

void Window::DidMoveCursor(double x_pos, double y_pos) {
  if (move_cursor_callback_ != nullptr) {
    move_cursor_callback_(x_pos, y_pos);
  }
}

void Window::DidScroll(double x_pos, double y_pos) {
  if (scroll_callback_ != nullptr) {
    scroll_callback_(x_pos, y_pos);
  }
}

Window::~Window() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

} /* namespace common */
} /* namespace jessie_steamer */
