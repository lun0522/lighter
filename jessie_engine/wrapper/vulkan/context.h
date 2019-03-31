//
//  context.h
//
//  Created by Pujun Lun on 3/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_ENGINE_WRAPPER_VULKAN_CONTEXT_H
#define JESSIE_ENGINE_WRAPPER_VULKAN_CONTEXT_H

#include <memory>
#include <string>

#include "jessie_engine/common/window.h"
#include "jessie_engine/wrapper/vulkan/basic_object.h"
#include "jessie_engine/wrapper/vulkan/pipeline.h"
#include "jessie_engine/wrapper/vulkan/render_pass.h"
#include "jessie_engine/wrapper/vulkan/swapchain.h"
#include "jessie_engine/wrapper/vulkan/validation.h"
#include "third_party/vulkan/vulkan.h"

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

  void set_allocator(VkAllocationCallbacks* allocator) {
    allocator_ = allocator;
  }

  void set_queues(const VkQueue& graphics_queue,
                  const VkQueue& present_queue) {
    queues_.set_queues(graphics_queue, present_queue);
  }

  void set_queue_family_indices(uint32_t graphics_index,
                                uint32_t present_index) {
    queues_.set_family_indices(graphics_index, present_index);
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

#endif /* JESSIE_ENGINE_WRAPPER_VULKAN_CONTEXT_H */
