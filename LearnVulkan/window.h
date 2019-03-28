//
//  window.h
//
//  Created by Pujun Lun on 3/27/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef PUBLIC_WINDOW_H
#define PUBLIC_WINDOW_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace wrapper {
namespace vulkan {

class Context;

} /* namespace vulkan */
} /* namespace wrapper */

namespace window {
namespace key_map {

enum class KeyMap {
  kKeyEscape,
  kKeyUp,
  kKeyDown,
  kKeyLeft,
  kKeyRight,
};

} /* namespace keymap */

using KeyCallback = std::function<void()>;
using CursorPosCallback = std::function<void(double x_pos, double y_pos)>;
using ScrollCallback = std::function<void(double x_pos, double y_pos)>;

class Window {
 public:
  virtual void Init(const std::string& name, glm::ivec2 screen_size) = 0;
  virtual VkSurfaceKHR CreateSurface(
      std::shared_ptr<wrapper::vulkan::Context> context) = 0;
  virtual void SetCursorHidden(bool hidden) = 0;
  virtual void RegisterKeyCallback(key_map::KeyMap key,
                                   KeyCallback callback) = 0;
  virtual void RegisterCursorPosCallback(CursorPosCallback callback) = 0;
  virtual void RegisterScrollCallback(ScrollCallback callback) = 0;
  virtual void PollEvents() = 0;
  virtual bool ShouldQuit() const = 0;
  virtual bool IsMinimized() const = 0;
  virtual bool IsResized() const { return is_resized_; }
  virtual void ResetResizedFlag() { is_resized_ = false; }
  virtual ~Window() {};

  virtual glm::ivec2 screen_size() const = 0;
  virtual glm::dvec2 mouse_pos() const = 0;

 protected:
  bool is_resized_ = false;
};

class GlfwWindow : public Window {
 public:
  GlfwWindow() = default;
  void Init(const std::string& name, glm::ivec2 screen_size) override;
  VkSurfaceKHR CreateSurface(
      std::shared_ptr<wrapper::vulkan::Context> context) override;
  void SetCursorHidden(bool hidden) override;
  void RegisterKeyCallback(key_map::KeyMap key, KeyCallback callback) override;
  void RegisterCursorPosCallback(CursorPosCallback callback) override;
  void RegisterScrollCallback(ScrollCallback callback) override;
  void PollEvents() override;
  bool ShouldQuit() const override { return glfwWindowShouldClose(window_); }
  bool IsMinimized() const override;
  ~GlfwWindow() override;

  // This class is neither copyable nor movable
  GlfwWindow(const GlfwWindow&) = delete;
  GlfwWindow& operator=(const GlfwWindow&) = delete;

  glm::ivec2 screen_size() const override;
  glm::dvec2 mouse_pos() const override;

 private:
  GLFWwindow* window_;
  std::unordered_map<int, std::function<void()>> key_callbacks_;
};

} /* namespace window */

#endif /* PUBLIC_WINDOW_H */
