//
//  context.h
//
//  Created by Pujun Lun on 3/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef VULKAN_WRAPPER_CONTEXT_H
#define VULKAN_WRAPPER_CONTEXT_H

#include <memory>
#include <string>

#include <vulkan/vulkan.hpp>

#include "basic_object.h"
#include "pipeline.h"
#include "render_pass.h"
#include "swapchain.h"
#include "util.h"
#include "validation.h"

class GLFWwindow;

namespace vulkan {
namespace wrapper {

class Context : public std::enable_shared_from_this<Context> {
 public:
  static std::shared_ptr<Context> CreateContext() {
    return std::shared_ptr<Context>{new Context};
  }
  void Init(const std::string& name,
            uint32_t width = 800,
            uint32_t height = 600);
  void Recreate();
  void Cleanup();
  bool ShouldQuit() const;
  void WaitIdle() const { vkDeviceWaitIdle(*device_); }
  ~Context();

  // This class is neither copyable nor movable
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  std::shared_ptr<Context> ptr()                  { return shared_from_this(); }
  bool& resized()                                 { return has_resized_; }
  VkExtent2D screen_size()                  const;
  const VkAllocationCallbacks* allocator()  const { return allocator_; }
  GLFWwindow* window()                      const { return window_; }
  const Instance& instance()                const { return instance_; }
  const Surface& surface()                  const { return surface_; }
  const PhysicalDevice& physical_device()   const { return physical_device_; }
  const Device& device()                    const { return device_; }
  const Swapchain& swapchain()              const { return swapchain_; }
  const RenderPass& render_pass()           const { return render_pass_; }
  const Queues& queues()                    const { return queues_; }
  Queues& queues()                                { return queues_; }

  void set_allocator(VkAllocationCallbacks* allocator) {
    allocator_ = allocator;
  }

 private:
  bool has_resized_{false};
  bool is_first_time_{true};
  VkAllocationCallbacks* allocator_{nullptr};
  GLFWwindow* window_;
  Instance instance_;
  Surface surface_;
  PhysicalDevice physical_device_;
  Device device_;
  Queues queues_;
  Swapchain swapchain_;
  RenderPass render_pass_;
#ifdef DEBUG
  DebugCallback callback_;
#endif /* DEBUG */

  Context() {}
  void InitWindow(const std::string& name, uint32_t width, uint32_t height);
  void InitVulkan();
};

} /* namespace wrapper */
} /* namespace vulkan */

#endif /* VULKAN_WRAPPER_CONTEXT_H */
