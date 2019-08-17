//
//  window.h
//
//  Created by Pujun Lun on 3/27/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_WINDOW_H
#define JESSIE_STEAMER_COMMON_WINDOW_H

#include <functional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "third_party/glm/glm.hpp"
#ifdef USE_VULKAN
#include "third_party/vulkan/vulkan.h"
// import GLFW after Vulkan
#endif /* USE_VULKAN */

// Forward declarations.
struct GLFWwindow;

namespace jessie_steamer {
namespace common {
namespace window_callback {

// Since GLFW requires C-style function callbacks, we use these functions to
// wrap the real callbacks stored in the Window instance.
void GlfwResizeWindowCallback(GLFWwindow* window, int width, int height);
void GlfwMoveCursorCallback(GLFWwindow* window, double x_pos, double y_pos);
void GlfwScrollCallback(GLFWwindow* window, double x_pos, double y_pos);

} /* namespace window_callback */

// Window backed by GLFW. It handles all interactions with the user, and the
// presentation of rendered frames.
class Window {
 public:
  // Returns the names of required extensions for creating the window.
  static const std::vector<const char*>& GetRequiredExtensions();

  using PressKeyCallback = std::function<void()>;
  using MoveCursorCallback = std::function<void(double x_pos, double y_pos)>;
  using ScrollCallback = std::function<void(double x_pos, double y_pos)>;

  // Keys that may be handled by the window. Callbacks of keys should be
  // registered with 'RegisterPressKeyCallback', otherwise, the window will not
  // respond to the press.
  enum class KeyMap { kEscape, kUp, kDown, kLeft, kRight };

  Window(const std::string& name, const glm::ivec2& screen_size);

  // This class is neither copyable nor movable.
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  ~Window();

#ifdef USE_VULKAN
  // Creates window surface for Vulkan applications.
  VkSurfaceKHR CreateSurface(const VkInstance& instance,
                             const VkAllocationCallbacks* allocator);
#endif /* USE_VULKAN */

  // Sets whether the cursor should be hidden.
  Window& SetCursorHidden(bool hidden);

  // Registers a callback that will be invoked when the 'key' is pressed.
  // 'callback' can be nullptr, which invalidates the callback previously
  // registered for 'key'.
  Window& RegisterPressKeyCallback(
      KeyMap key, const PressKeyCallback& callback);

  // Registers a callback that will be invoked when the cursor is moved.
  // 'callback' can be nullptr, which invalidates callbacks registered before.
  Window& RegisterMoveCursorCallback(MoveCursorCallback callback);

  // Registers a callback that will be invoked when the user scrolls.
  // 'callback' can be nullptr, which invalidates callbacks registered before.
  Window& RegisterScrollCallback(ScrollCallback callback);

  // Processes user inputs to the window. Callbacks will be invoked if
  // conditions are satisfied.
  void ProcessUserInputs() const;

  // Resets internal states and returns the current size of window.
  // This should be called when the window is resized. It will not return if the
  // window is minimized, until the window appears on the screen again.
  glm::ivec2 Recreate();

  // Returns whether the window has received the signal that indicates it should
  // be closed.
  bool ShouldQuit() const;

  // Returns the size of window.
  glm::ivec2 GetScreenSize() const;

  // Returns the position of cursor.
  glm::dvec2 GetCursorPos() const;

  // Accessors.
  bool is_resized() const { return is_resized_; }

 private:
  friend void window_callback::GlfwResizeWindowCallback(GLFWwindow*, int, int);
  friend void window_callback::GlfwMoveCursorCallback(
      GLFWwindow*, double, double);
  friend void window_callback::GlfwScrollCallback(GLFWwindow*, double, double);

  // These functions handles window events and invoke callbacks if necessary.
  void DidResizeWindow();
  void DidMoveCursor(double x_pos, double y_pos);
  void DidScroll(double x_pos, double y_pos);

  // Whether the window has been resized.
  bool is_resized_ = false;

  // Pointer to the backing GLFWwindow.
  GLFWwindow* window_ = nullptr;

  // Invoked when the cursor is moved.
  MoveCursorCallback move_cursor_callback_;

  // Invoked when the user scrolls.
  ScrollCallback scroll_callback_;

  // Maps GLFW keys to their callbacks.
  absl::flat_hash_map<int, std::function<void()>> press_key_callbacks_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_WINDOW_H */
