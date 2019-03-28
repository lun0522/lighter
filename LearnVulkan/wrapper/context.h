//
//  context.h
//
//  Created by Pujun Lun on 3/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_CONTEXT_H
#define WRAPPER_VULKAN_CONTEXT_H

#include <memory>
#include <string>

#include <vulkan/vulkan.hpp>

#include "basic_object.h"
#include "pipeline.h"
#include "render_pass.h"
#include "swapchain.h"
#include "validation.h"
#include "window.h"

namespace wrapper {
namespace vulkan {

using SharedContext = std::shared_ptr<Context>;

class Context : public std::enable_shared_from_this<Context> {
 public:
  static std::shared_ptr<Context> CreateContext() {
    return std::shared_ptr<Context>{new Context};
  }
  Context() = default;
  void Init(const std::string& name = "", int width = 800, int height = 600);
  void Recreate();
  void WaitIdle() const { vkDeviceWaitIdle(*device_); }

  // This class is neither copyable nor movable
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  std::shared_ptr<Context> ptr()                  { return shared_from_this(); }
  window::Window& window()                        { return window_; }
  const VkAllocationCallbacks* allocator()  const { return allocator_; }
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
  bool is_first_time_ = true;
  window::GlfwWindow window_;
  VkAllocationCallbacks* allocator_ = nullptr;
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

  void Cleanup();
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_CONTEXT_H */
