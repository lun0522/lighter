//
//  context.h
//
//  Created by Pujun Lun on 3/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_CONTEXT_H
#define WRAPPER_VULKAN_CONTEXT_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "basic_object.h"
#include "pipeline.h"
#include "render_pass.h"
#include "swapchain.h"
#include "validation.h"

class GLFWwindow;

namespace wrapper {
namespace vulkan {
namespace keymap {

enum KeyMap {
  kKeyEscape = GLFW_KEY_ESCAPE,
  kKeyUp     = GLFW_KEY_UP,
  kKeyDown   = GLFW_KEY_DOWN,
  kKeyLeft   = GLFW_KEY_LEFT,
  kKeyRight  = GLFW_KEY_RIGHT,
};

} /* namespace keymap */

using SharedContext = std::shared_ptr<Context>;
using MouseMoveCallback = std::function<void(double x_pos, double y_pos)>;
using MouseScrollCallback = std::function<void(double x_pos, double y_pos)>;

class Context : public std::enable_shared_from_this<Context> {
 public:
  static std::shared_ptr<Context> CreateContext() {
    return std::shared_ptr<Context>{new Context};
  }
  void Init(const std::string& name,
            uint32_t width = 800,
            uint32_t height = 600);
  void Recreate();
  void RegisterMouseMoveCallback(const MouseMoveCallback& callback);
  void RegisterMouseScrollCallback(const MouseScrollCallback& callback);
  void RegisterKeyCallback(keymap::KeyMap key,
                           const std::function<void()>& callback);
  void UnregisterKeyCallback(keymap::KeyMap key);
  void PollEvents();
  bool ShouldQuit() const { return glfwWindowShouldClose(window_); }
  void WaitIdle()   const { vkDeviceWaitIdle(*device_); }
  ~Context();

  // This class is neither copyable nor movable
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  std::shared_ptr<Context> ptr()                  { return shared_from_this(); }
  bool& resized()                                 { return has_resized_; }
  glm::vec2 screen_size()                   const;
  glm::vec2 mouse_pos()                     const;
  const VkAllocationCallbacks* allocator()  const { return allocator_; }
  GLFWwindow* window()                      const { return window_; }
  const Instance& instance()                const { return instance_; }
  const Surface& surface()                  const { return surface_; }
  const PhysicalDevice& physical_device()   const { return physical_device_; }
  const Device& device()                    const { return device_; }
  const Swapchain& swapchain()              const { return swapchain_; }
  const RenderPass& render_pass()           const { return render_pass_; }
  RenderPass& render_pass()                       { return render_pass_; }
  const Queues& queues()                    const { return queues_; }
  Queues& queues()                                { return queues_; }

  void set_allocator(VkAllocationCallbacks* allocator) {
    allocator_ = allocator;
  }

 private:
  bool has_resized_ = false;
  bool is_first_time_ = true;
  VkAllocationCallbacks* allocator_ = nullptr;
  GLFWwindow* window_;
  Instance instance_;
  Surface surface_;
  PhysicalDevice physical_device_;
  Device device_;
  Queues queues_;
  Swapchain swapchain_;
  RenderPass render_pass_;
  std::unordered_map<keymap::KeyMap, std::function<void()>> key_callbacks_;
#ifdef DEBUG
  DebugCallback callback_;
#endif /* DEBUG */

  Context() {}
  void InitWindow(const std::string& name, uint32_t width, uint32_t height);
  void InitVulkan();
  void Cleanup();
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_CONTEXT_H */
