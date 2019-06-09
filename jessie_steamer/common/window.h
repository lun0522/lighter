//
//  window.h
//
//  Created by Pujun Lun on 3/27/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_WINDOW_H
#define JESSIE_STEAMER_COMMON_WINDOW_H

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "third_party/glm/glm.hpp"
#ifdef USE_VULKAN
#include "third_party/vulkan/vulkan.h"
// import GLFW after Vulkan
#endif /* USE_VULKAN */
#include "third_party/glfw/glfw3.h"

namespace jessie_steamer {
namespace common {

class Window {
 public:
  using KeyCallback = std::function<void()>;
  using CursorMoveCallback = std::function<void(double x_pos, double y_pos)>;
  using ScrollCallback = std::function<void(double x_pos, double y_pos)>;

  enum class KeyMap { kEscape, kUp, kDown, kLeft, kRight };

  Window() = default;

  // This class is neither copyable nor movable.
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  ~Window();

  void Init(const std::string& name, glm::ivec2 screen_size);
#ifdef USE_VULKAN
  VkSurfaceKHR CreateSurface(const VkInstance& instance,
                             const VkAllocationCallbacks* allocator);
#endif /* USE_VULKAN */
  void SetCursorHidden(bool hidden);
  void RegisterKeyCallback(KeyMap key, const KeyCallback& callback);
  void RegisterCursorMoveCallback(CursorMoveCallback callback);
  void RegisterScrollCallback(ScrollCallback callback);
  void PollEvents();
  bool ShouldQuit() const { return glfwWindowShouldClose(window_); }
  glm::ivec2 GetScreenSize() const;
  glm::dvec2 GetCursorPos() const;
  bool IsMinimized() const;
  void ResetResizedFlag() { is_resized_ = false; }

#ifdef USE_VULKAN
  static const std::vector<const char*>& required_extensions();
#endif /* USE_VULKAN */
  bool is_resized() const { return is_resized_; }

 private:
  bool is_resized_ = false;
  GLFWwindow* window_ = nullptr;
  absl::flat_hash_map<int, std::function<void()>> key_callbacks_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_WINDOW_H */
