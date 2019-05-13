//
//  context.h
//
//  Created by Pujun Lun on 3/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_CONTEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_CONTEXT_H

#include <memory>
#include <string>

#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/basic_object.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/swapchain.h"
#include "jessie_steamer/wrapper/vulkan/validation.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

using SharedContext = std::shared_ptr<Context>;

class Context : public std::enable_shared_from_this<Context> {
 public:
  static SharedContext GetContext() { return std::make_shared<Context>(); }

  Context() = default;

  // This class is neither copyable nor movable
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  void Init(const std::string& name = "", int width = 800, int height = 600);
  void Recreate();
  void WaitIdle() const { vkDeviceWaitIdle(*device_); }

  SharedContext ptr()                             { return shared_from_this(); }
  common::Window& window()                        { return window_; }
  const VkAllocationCallbacks* allocator()  const { return allocator_; }
  const Instance& instance()                const { return instance_; }
  const Surface& surface()                  const { return surface_; }
  const PhysicalDevice& physical_device()   const { return physical_device_; }
  const Device& device()                    const { return device_; }
  const Swapchain& swapchain()              const { return swapchain_; }
  const RenderPass& render_pass()           const { return render_pass_; }
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
  void Cleanup();

  bool is_first_time_ = true;
  common::GlfwWindow window_;
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
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_CONTEXT_H */
