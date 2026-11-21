//
//  context.h
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_CONTEXT_H
#define LIGHTER_RENDERER_VK_CONTEXT_H

#include <vk_mem_alloc.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "lighter/base/util.h"
#include "lighter/base/window.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/basic.h"

namespace lighter::renderer::vk {

// Forward declarations.
class Context;

using SharedContext = std::shared_ptr<Context>;

// Owns core Vulkan handles (instance, surfaces, physical/logical device,
// queues, VMA allocator) and manages timeline-based deferred resource cleanup.
class Context : public std::enable_shared_from_this<Context> {
 public:
  // Specifies how to release an expired resource.
  using ReleaseExpiredResourceOp = std::function<void(const Context& context)>;

  static SharedContext CreateContext(
      const char* application_name,
      const std::optional<ir::debug_message::Config>& debug_message_config,
      std::span<const base::Window* const> windows) {
    return std::shared_ptr<Context>(
        new Context{application_name, debug_message_config, windows});
  }

  // This class is neither copyable nor movable.
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  ~Context();

  // Convenience functions for destroying Vulkan objects.
  template <typename T>
  void InstanceDestroy(T t) const {
    instance()->destroy(t, *host_allocator());
  }
  template <typename T>
  void DeviceDestroy(T t) const {
    device()->destroy(t, *host_allocator());
  }

  // Records commands on a transient command buffer, submits to the graphics
  // queue, and waits until completion.
  void RunOneTimeGraphicsCommand(
      const std::function<void(intl::CommandBuffer)>& record_commands) const;

  // Records an operation that releases an expired resource once the GPU passes
  // all previously submitted timeline values (or when WaitIdle() runs).
  void AddReleaseExpiredResourceOp(ReleaseExpiredResourceOp&& op) {
    release_expired_rsrc_ops_.push_back(
        {submitted_timeline_value_, std::move(op)});
  }

  // Advances and returns the next monotonically increasing timeline value for
  // an upcoming queue submission.
  uint64_t NextTimelineValue() { return ++submitted_timeline_value_; }

  // Releases expired resources whose timeline value has completed on the GPU.
  void ReleaseCompletedResources();

  // Blocks until the device is idle and releases all remaining expired
  // resources (used on swapchain recreation and shutdown).
  void WaitIdle();

  // Accessors.
  const HostMemoryAllocator& host_allocator() const { return *host_allocator_; }
  const Instance& instance() const { return *instance_; }
  const Surface& surface(int window_index) const {
    return *surfaces_.at(window_index);
  }
  Surface& mutable_surface(int window_index) {
    return *surfaces_.at(window_index);
  }
  const PhysicalDevice& physical_device() const { return *physical_device_; }
  const Device& device() const { return *device_; }
  const Queues& queues() const { return *queues_; }
  intl::Semaphore timeline_semaphore() const { return timeline_semaphore_; }
  VmaAllocator vma_allocator() const { return vma_allocator_; }

 private:
  // Deferred cleanup callback paired with the timeline value after which it may
  // run.
  struct PendingResourceRelease {
    uint64_t retire_value = 0;
    ReleaseExpiredResourceOp op;
  };

  Context(const char* application_name,
          const std::optional<ir::debug_message::Config>& debug_message_config,
          std::span<const base::Window* const> windows);

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

  // Timeline semaphore tracking GPU completion for deferred resource cleanup.
  intl::Semaphore timeline_semaphore_;
  uint64_t submitted_timeline_value_ = 0;

  // VMA GPU memory allocator.
  VmaAllocator vma_allocator_ = VK_NULL_HANDLE;

  // Ops delayed until their associated timeline value completes (or WaitIdle).
  std::vector<PendingResourceRelease> release_expired_rsrc_ops_;
};

// Base mixin providing Vulkan objects shared ownership of Context and
// accessors.
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
  VmaAllocator vma_allocator() const { return context_->vma_allocator(); }

 protected:
  const SharedContext context_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_CONTEXT_H
