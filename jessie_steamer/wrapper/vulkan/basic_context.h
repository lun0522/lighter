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
#include "absl/memory/memory.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_object.h"
#ifndef NDEBUG
#include "jessie_steamer/wrapper/vulkan/validation.h"
#endif /* !NDEBUG */
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class BasicContext;
using SharedBasicContext = std::shared_ptr<BasicContext>;

using ReleaseExpiredResourceOp = std::function<void()>;

struct WindowSupport {
  const VkSurfaceKHR* surface;
  const std::vector<const char*>& window_extensions;
  const std::vector<const char*>& swapchain_extensions;
  const std::function<void(const VkAllocationCallbacks* allocator,
                           const VkInstance& instance)>& create_surface;
};

class BasicContext : public std::enable_shared_from_this<BasicContext> {
 public:
  static SharedBasicContext GetContext() {
    return std::make_shared<BasicContext>();
  }

  BasicContext() = default;

  // This class is neither copyable nor movable.
  BasicContext(const BasicContext&) = delete;
  BasicContext& operator=(const BasicContext&) = delete;

  void Init(const VkAllocationCallbacks* allocator,
            const absl::optional<WindowSupport>& window_support) {
    allocator_ = allocator;
    instance_.Init(ptr(), window_support);
#ifndef NDEBUG
    // Relay debug messages back to application.
    debug_callback_.Init(ptr(),
                         message_severity::kWarning
                             | message_severity::kError,
                         message_type::kGeneral
                             | message_type::kValidation
                             | message_type::kPerformance);
#endif /* !NDEBUG */
    // Create surface if required. Caller should destruct it at the end.
    if (window_support.has_value()) {
      window_support.value().create_surface(allocator_, *instance_);
    }
    const auto queue_family_indices =
        physical_device_.Init(ptr(), window_support);
    queues_ = device_.Init(ptr(), queue_family_indices, window_support);
  }

  void AddReleaseExpiredResourceOp(ReleaseExpiredResourceOp&& op) {
    release_expired_rsrc_ops_.emplace_back(std::move(op));
  }

  void WaitIdle() {
    vkDeviceWaitIdle(*device_);
    if (!release_expired_rsrc_ops_.empty()) {
      for (const auto& op : release_expired_rsrc_ops_) { op(); }
      release_expired_rsrc_ops_.clear();
    }
  }

  SharedBasicContext ptr()                        { return shared_from_this(); }
  const VkAllocationCallbacks* allocator()  const { return allocator_; }
  const Instance& instance()                const { return instance_; }
  const PhysicalDevice& physical_device()   const { return physical_device_; }
  const Device& device()                    const { return device_; }
  const Queues& queues() const {
    ASSERT_NON_NULL(queues_, "Queues have not been initialized");
    return *queues_;
  }

  const VkPhysicalDeviceLimits& limits() const {
    return physical_device_.limits();
  }

 private:
  const VkAllocationCallbacks* allocator_ = nullptr;
  Instance instance_;
  PhysicalDevice physical_device_;
  Device device_;
  std::unique_ptr<Queues> queues_;
#ifndef NDEBUG
  DebugCallback debug_callback_;
#endif /* !NDEBUG */
  std::vector<ReleaseExpiredResourceOp> release_expired_rsrc_ops_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_CONTEXT_H */
