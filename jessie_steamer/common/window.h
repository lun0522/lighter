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

#include "third_party/absl/container/flat_hash_map.h"
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
void GlfwMouseButtonCallback(
    GLFWwindow* window, int button, int action, int mods);

} /* namespace window_callback */

// This class is backed by GLFW. It handles all interactions with the user, and
// the presentation of rendered frames.
class Window {
 public:
  // Callbacks used for responding to user inputs.
  using PressKeyCallback = std::function<void()>;
  using MoveCursorCallback = std::function<void(double x_pos, double y_pos)>;
  using ScrollCallback = std::function<void(double x_pos, double y_pos)>;
  using MouseButtonCallback = std::function<void(bool is_left, bool is_press)>;

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
  Window& RegisterPressKeyCallback(KeyMap key, PressKeyCallback&& callback);

  // Registers a callback that will be invoked when the cursor is moved.
  // 'callback' can be nullptr, which invalidates callbacks registered before.
  Window& RegisterMoveCursorCallback(MoveCursorCallback&& callback);

  // Registers a callback that will be invoked when the user scrolls.
  // 'callback' can be nullptr, which invalidates callbacks registered before.
  Window& RegisterScrollCallback(ScrollCallback&& callback);

  // Registers a callback that will be invoked when the mouse button is pressed
  // or released. 'callback' can be nullptr, which invalidates callbacks
  // registered before.
  Window& RegisterMouseButtonCallback(MouseButtonCallback&& callback);

  // Processes user inputs to the window. Callbacks will be invoked if
  // conditions are satisfied.
  void ProcessUserInputs() const;

  // Resets internal states and returns the current size of screen frame.
  // This should be called when the window is resized. It will not return if the
  // window is minimized, until the window appears on the screen again.
  glm::ivec2 Recreate();

  // Returns whether the window has received the signal that indicates it should
  // be closed.
  bool ShouldQuit() const;

#ifdef USE_VULKAN
  // Returns the names of required extensions for creating the window.
  static const std::vector<const char*>& GetRequiredExtensions();
#endif /* USE_VULKAN */

  // Returns the frame size of window. Note that this is different from the
  // window size on retina displays.
  glm::ivec2 GetFrameSize() const;

  // Returns the position of cursor.
  glm::dvec2 GetCursorPos() const;

  // Returns the position of cursor in the normalized screen coordinate.
  // Note that this can be different from the normalized device coordinate,
  // since we may not render to full screen.
  glm::dvec2 GetNormalizedCursorPos() const;

  // Accessors.
  bool is_resized() const { return is_resized_; }

 private:
  friend void window_callback::GlfwResizeWindowCallback(GLFWwindow*, int, int);
  friend void window_callback::GlfwMoveCursorCallback(
      GLFWwindow*, double, double);
  friend void window_callback::GlfwScrollCallback(GLFWwindow*, double, double);
  friend void window_callback::GlfwMouseButtonCallback(
      GLFWwindow* window, int button, int action, int mods);

  // These functions handles window events and invoke callbacks if necessary.
  void DidResizeWindow();
  void DidMoveCursor(double x_pos, double y_pos);
  void DidScroll(double x_pos, double y_pos);
  void DidClickMouse(bool is_left, bool is_press);

  // Updates 'retina_ratio_'. On retina displays, the framebuffer size will be
  // greater than the window size. This should be called after the window is
  // created, and whenever it is recreated.
  void UpdateRetinaRatio();

  // Whether the window has been resized.
  bool is_resized_ = false;

  // Pointer to the backing GLFWwindow.
  GLFWwindow* window_ = nullptr;

  // Ratio between the screen frame size and window size. This is used to
  // calibrate the cursor position reported by the window manager.
  glm::ivec2 retina_ratio_;

  // Invoked when the cursor is moved.
  MoveCursorCallback move_cursor_callback_;

  // Invoked when the user scrolls.
  ScrollCallback scroll_callback_;

  // Invoked wen the mouse button is pressed or released.
  MouseButtonCallback mouse_button_callback_;

  // Maps GLFW keys to their callbacks.
  absl::flat_hash_map<int, std::function<void()>> press_key_callbacks_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_WINDOW_H */
