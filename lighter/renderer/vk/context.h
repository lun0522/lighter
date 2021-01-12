//
//  context.h
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_CONTEXT_H
#define LIGHTER_RENDERER_VK_CONTEXT_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "lighter/common/window.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/basic.h"
#include "lighter/renderer/vk/debug_callback.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk {

// Forward declarations.
class Context;

using SharedContext = std::shared_ptr<Context>;

class Context : public std::enable_shared_from_this<Context> {
 public:
  // Specifies how to release an expired resource.
  using ReleaseExpiredResourceOp = std::function<void(const Context& context)>;

  static SharedContext CreateContext(
      std::string_view application_name,
      const std::optional<debug_message::Config>& debug_message_config,
      absl::Span<const common::Window* const> windows,
      absl::Span<const char* const> swapchain_extensions) {
    return std::shared_ptr<Context>(
        new Context{application_name, debug_message_config, windows,
                    swapchain_extensions});
  }

  // This class is neither copyable nor movable.
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  // Records an operation that releases an expired resource, so that it can be
  // executed once the device becomes idle. This is used for resources that can
  // be released only when the device is no longer using it.
  void AddReleaseExpiredResourceOp(ReleaseExpiredResourceOp&& op) {
    release_expired_rsrc_ops_.push_back(std::move(op));
  }

  // TODO: Avoid this.
  // Waits for the device idle, and releases expired resources.
  // This should be called in the middle of the program when we want to destroy
  // and recreate some resources, such as the swapchain and data buffers.
  void WaitIdle() {
    device_->WaitIdle();
    for (const auto& op : release_expired_rsrc_ops_) {
      op(*this);
    }
    release_expired_rsrc_ops_.clear();
  }

  // Accessors.
  bool is_validation_enabled() const { return debug_callback_ != nullptr; }
  const HostMemoryAllocator& host_allocator() const { return host_allocator_; }
  const Instance& instance() const { return *instance_; }
  const Surface& surface(int window_index) const {
    return *surfaces_.at(window_index);
  }
  const PhysicalDevice& physical_device() const { return *physical_device_; }
  const Device& device() const { return *device_; }
  const Queues& queues() const { return *queues_; }

 private:
  Context(std::string_view application_name,
          const std::optional<debug_message::Config>& debug_message_config,
          absl::Span<const common::Window* const> windows,
          absl::Span<const char* const> swapchain_extensions);

  // Wrapper of VkAllocationCallbacks.
  const HostMemoryAllocator host_allocator_;

  // Wrapper of VkInstance.
  std::unique_ptr<Instance> instance_;

  // Wrapper of VkDebugUtilsMessengerEXT.
  std::unique_ptr<DebugCallback> debug_callback_;

  // Wrapper of VkSurfaceKHR.
  std::vector<std::unique_ptr<Surface>> surfaces_;

  // Wrapper of VkPhysicalDevice.
  std::unique_ptr<PhysicalDevice> physical_device_;

  // Wrapper of VkDevice.
  std::unique_ptr<Device> device_;

  // Wrapper of VkQueue.
  std::unique_ptr<Queues> queues_;

  // Ops that are delayed to be executed until the device becomes idle.
  std::vector<ReleaseExpiredResourceOp> release_expired_rsrc_ops_;
};

}  // namespace vk::renderer::lighter

#endif  // LIGHTER_RENDERER_VK_CONTEXT_H
