//
//  basic_context.h
//
//  Created by Pujun Lun on 3/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_CONTEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_CONTEXT_H

#include <functional>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "jessie_steamer/wrapper/vulkan/basic_object.h"
#ifndef NDEBUG
#include "jessie_steamer/wrapper/vulkan/validation.h"
#endif /* !NDEBUG */
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// Forward declarations.
class BasicContext;

using SharedBasicContext = std::shared_ptr<BasicContext>;
using ReleaseExpiredResourceOp = std::function<void(const SharedBasicContext&)>;

// Information that we need to use a window. We need to make sure the window and
// swapchain are supported by checking supports for relevant extensions.
// The caller does not need to initialize 'surface', but should be responsible
// for destructing 'surface' at the end.
struct WindowSupport {
  VkSurfaceKHR* surface;
  const std::vector<const char*>& window_extensions;
  const std::vector<const char*>& swapchain_extensions;
  const std::function<void(
      const VkInstance& instance,
      const VkAllocationCallbacks* allocator)>& create_surface;
};

class BasicContext : public std::enable_shared_from_this<BasicContext> {
 public:
  // Returns a new instance of BasicContext.
  static SharedBasicContext GetContext() {
    return std::make_shared<BasicContext>();
  }

  // Instead of allocating BasicContext on stack, the user should prefer to call
  // GetContext() to get a context whose lifetime is self-managed.
  BasicContext() = default;

  // This class is neither copyable nor movable.
  BasicContext(const BasicContext&) = delete;
  BasicContext& operator=(const BasicContext&) = delete;

  // Initializes basic objects.
  void Init(const absl::optional<WindowSupport>& window_support) {
    instance_.Init(self(), window_support);
#ifndef NDEBUG
    debug_callback_.Init(self(),
                         message_severity::kWarning
                             | message_severity::kError,
                         message_type::kGeneral
                             | message_type::kValidation
                             | message_type::kPerformance);
#endif /* !NDEBUG */
    if (window_support.has_value()) {
      window_support.value().create_surface(*instance_, *allocator_);
    }
    const auto queue_family_indices =
        physical_device_.Init(self(), window_support);
    device_.Init(self(), queue_family_indices, window_support);
    queues_.Init(self(), queue_family_indices);
  }

  // Records an operation that releases an expired resource, so that it can be
  // executed once the graphics device becomes idle. This is used for resources
  // that can be released only when the graphics device is no longer using it.
  void AddReleaseExpiredResourceOp(ReleaseExpiredResourceOp&& op) {
    release_expired_rsrc_ops_.emplace_back(std::move(op));
  }

  // Waits for the graphics device becomes idle, and releases expired resources.
  void WaitIdle() {
    device_.WaitIdle();
    if (!release_expired_rsrc_ops_.empty()) {
      for (const auto& op : release_expired_rsrc_ops_) { op(self()); }
      release_expired_rsrc_ops_.clear();
    }
  }

  // Accessors.
  SharedBasicContext self() { return shared_from_this(); }
  const Instance& instance() const { return instance_; }
  const PhysicalDevice& physical_device() const { return physical_device_; }
  const Device& device() const { return device_; }
  const Queues& queues() const { return queues_; }
  const VkPhysicalDeviceLimits& device_limits() const {
    return physical_device_.limits();
  }
  const HostMemoryAllocator& allocator() const { return allocator_; }

 private:
  // Wrapper of VkInstance.
  Instance instance_;

  // Wrapper of VkPhysicalDevice.
  PhysicalDevice physical_device_;

  // Wrapper of VkDevice.
  Device device_;

  // Wrapper of VkQueue.
  Queues queues_;

  // Wrapper of VkAllocationCallbacks.
  HostMemoryAllocator allocator_;

#ifndef NDEBUG
  // Wrapper of VkDebugUtilsMessengerEXT.
  DebugCallback debug_callback_;
#endif /* !NDEBUG */

  // Holds operations that are delayed to be executed until the graphics device
  // becomes idle.
  std::vector<ReleaseExpiredResourceOp> release_expired_rsrc_ops_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_CONTEXT_H */
