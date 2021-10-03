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
#include <vector>

#include "lighter/common/util.h"
#include "lighter/common/window.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/basic.h"
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
      const char* application_name,
      const std::optional<ir::debug_message::Config>& debug_message_config,
      absl::Span<const common::Window* const> windows) {
    return std::shared_ptr<Context>(
        new Context{application_name, debug_message_config, windows});
  }

  // This class is neither copyable nor movable.
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  ~Context() {
    WaitIdle();
#ifndef NDEBUG
    LOG_INFO << "Context destructed";
#endif  // DEBUG
  }

  // Convenience functions for destroying Vulkan objects.
  template <typename T>
  void InstanceDestroy(T t) const {
    instance()->destroy(t, *host_allocator());
  }
  template <typename T>
  void DeviceDestroy(T t) const {
    device()->destroy(t, *host_allocator());
  }

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
  const HostMemoryAllocator& host_allocator() const { return *host_allocator_; }
  const Instance& instance() const { return *instance_; }
  const Surface& surface(int window_index) const {
    return *surfaces_.at(window_index);
  }
  const PhysicalDevice& physical_device() const { return *physical_device_; }
  const Device& device() const { return *device_; }
  const Queues& queues() const { return *queues_; }

 private:
  Context(const char* application_name,
          const std::optional<ir::debug_message::Config>& debug_message_config,
          absl::Span<const common::Window* const> windows);

  // Wrapper of VkAllocationCallbacks.
  std::unique_ptr<HostMemoryAllocator> host_allocator_;

  // Wrapper of VkInstance.
  std::unique_ptr<Instance> instance_;

  // Wrapper of VkDebugUtilsMessengerEXT.
  std::unique_ptr<DebugMessenger> debug_messenger_;

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

class WithSharedContext {
 public:
  explicit WithSharedContext(const SharedContext& context)
      : context_{FATAL_IF_NULL(context)} {}

  // This class is neither copyable nor movable.
  WithSharedContext(const WithSharedContext&) = delete;
  WithSharedContext& operator=(const WithSharedContext&) = delete;

  virtual ~WithSharedContext() = default;

  // Accessors.
  intl::Device vk_device() const { return *context_->device(); }
  intl::Optional<const intl::AllocationCallbacks> vk_host_allocator() const {
    return *context_->host_allocator();
  }

 protected:
  const SharedContext context_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_CONTEXT_H
