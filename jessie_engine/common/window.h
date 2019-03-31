//
//  window.h
//
//  Created by Pujun Lun on 3/27/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_ENGINE_COMMON_WINDOW_H
#define JESSIE_ENGINE_COMMON_WINDOW_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "external/lib-glm/glm.hpp"
#define GLFW_INCLUDE_VULKAN
#include "third_party/glfw/glfw3.h"
#include "third_party/vulkan/vulkan.h"

namespace wrapper {
namespace vulkan {

class Context;

} /* namespace vulkan */
} /* namespace wrapper */

namespace window {
namespace key_map {

enum class KeyMap { kEscape, kUp, kDown, kLeft, kRight };

} /* namespace keymap */

using KeyCallback = std::function<void()>;
using CursorMoveCallback = std::function<void(double x_pos, double y_pos)>;
using ScrollCallback = std::function<void(double x_pos, double y_pos)>;

class Window {
 public:
  virtual void Init(const std::string& name, glm::ivec2 screen_size) = 0;
  virtual VkSurfaceKHR CreateSurface(
      const VkInstance& instance,
      const VkAllocationCallbacks* allocator) = 0;
  virtual void SetCursorHidden(bool hidden) = 0;
  virtual void RegisterKeyCallback(key_map::KeyMap key,
                                   KeyCallback callback) = 0;
  virtual void RegisterCursorMoveCallback(CursorMoveCallback callback) = 0;
  virtual void RegisterScrollCallback(ScrollCallback callback) = 0;
  virtual void PollEvents() = 0;
  virtual bool ShouldQuit() const = 0;
  virtual bool IsMinimized() const = 0;
  virtual bool IsResized() const { return is_resized_; }
  virtual void ResetResizedFlag() { is_resized_ = false; }
  virtual ~Window() {};

  virtual glm::ivec2 screen_size() const = 0;
  virtual glm::dvec2 cursor_pos() const = 0;

 protected:
  bool is_resized_ = false;
};

class GlfwWindow : public Window {
 public:
  GlfwWindow() = default;
  void Init(const std::string& name, glm::ivec2 screen_size) override;
  VkSurfaceKHR CreateSurface(const VkInstance& instance,
                             const VkAllocationCallbacks* allocator) override;
  void SetCursorHidden(bool hidden) override;
  void RegisterKeyCallback(key_map::KeyMap key, KeyCallback callback) override;
  void RegisterCursorMoveCallback(CursorMoveCallback callback) override;
  void RegisterScrollCallback(ScrollCallback callback) override;
  void PollEvents() override;
  bool ShouldQuit() const override { return glfwWindowShouldClose(window_); }
  bool IsMinimized() const override;
  ~GlfwWindow() override;

  // This class is neither copyable nor movable
  GlfwWindow(const GlfwWindow&) = delete;
  GlfwWindow& operator=(const GlfwWindow&) = delete;

  glm::ivec2 screen_size() const override;
  glm::dvec2 cursor_pos() const override;

 private:
  GLFWwindow* window_;
  std::unordered_map<int, std::function<void()>> key_callbacks_;
};

} /* namespace window */

#endif /* JESSIE_ENGINE_COMMON_WINDOW_H */
