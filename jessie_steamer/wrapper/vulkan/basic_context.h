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
#include <string>
#include <vector>

#include "jessie_steamer/common/ref_count.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_object.h"
#ifndef NDEBUG
#include "jessie_steamer/wrapper/vulkan/validation.h"
#endif /* !NDEBUG */
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// Forward declarations.
class BasicContext;

// Each wrapper class would hold a shared pointer to the context.
using SharedBasicContext = std::shared_ptr<BasicContext>;

// Information that we need to use a window. We need to make sure the window and
// swapchain are supported by checking supports for relevant extensions.
// The user should leave 'surface' uninitialized when instantiate this class,
// and provide 'create_surface' that initializes 'surface'.
// Note that only VkAllocationCallbacks and VkInstance are guaranteed to exist
// when 'create_surface' is called.
struct WindowSupport {
  absl::Span<const char* const> window_extensions;
  absl::Span<const char* const> swapchain_extensions;
  const VkSurfaceKHR& surface;
  const std::function<void(const BasicContext* context)>& create_surface;
};

// Members of this class are required for all graphics applications.
// Instead of allocating on stack, the user should call
// 'BasicContext::GetContext' to get an instance whose lifetime is self-managed.
class BasicContext : public std::enable_shared_from_this<BasicContext> {
 public:
  // Specifies how to release an expired resource.
  using ReleaseExpiredResourceOp = std::function<void(const BasicContext&)>;

  // Returns a new instance of BasicContext.
  template <typename... Args>
  static SharedBasicContext GetContext(Args&&... args) {
    return std::shared_ptr<BasicContext>(
        new BasicContext{std::forward<Args>(args)...});
  }

  // This class is neither copyable nor movable.
  BasicContext(const BasicContext&) = delete;
  BasicContext& operator=(const BasicContext&) = delete;

#ifndef NDEBUG
  ~BasicContext() { LOG_INFO << "Context destructed properly"; }
#endif  /* !NDEBUG */

  // Records an operation that releases an expired resource, so that it can be
  // executed once the graphics device becomes idle. This is used for resources
  // that can be released only when the graphics device is no longer using it.
  void AddReleaseExpiredResourceOp(ReleaseExpiredResourceOp&& op) {
    release_expired_rsrc_ops_.push_back(std::move(op));
  }

  // Registers a type of auto release pool. This should be called if the
  // reference counted objects should be constructed and destructed with this
  // context. Before exiting the program, we need to make sure all auto release
  // pools have been released, so do the associated resources, so that this
  // context can be destructed properly.
  template <typename RefCountedObjectType>
  void RegisterAutoReleasePool(const std::string& pool_name) {
    static bool first_time = true;
    if (first_time) {
      first_time = false;
      check_no_active_auto_release_pool_ops_.push_back([pool_name]() {
        if (RefCountedObjectType::has_active_auto_release_pool()) {
          FATAL(absl::StrFormat("Number of '%s' auto release pool is non-zero",
                                pool_name));
        }
      });
    }
  }

  // Waits for the graphics device becomes idle, and releases expired resources.
  // This should be called in the middle of the program when we want to destroy
  // and recreate some resources, such as the swapchain and data buffers.
  void WaitIdle() {
    device_.WaitIdle();
    if (!release_expired_rsrc_ops_.empty()) {
      for (const auto& op : release_expired_rsrc_ops_) { op(*this); }
      release_expired_rsrc_ops_.clear();
    }
  }

  // Waits for the graphics device becomes idle, and releases expired resources.
  // This should be called when the program is about to end, and right before
  // other resources get destroyed.
  void OnExit() {
    device_.WaitIdle();
    for (const auto& op : release_expired_rsrc_ops_) { op(*this); }
    for (const auto& op : check_no_active_auto_release_pool_ops_) { op(); }
  }

  // Returns unique queue family indices.
  std::vector<uint32_t> GetUniqueFamilyIndices() const {
    return physical_device_.queue_family_indices().GetUniqueFamilyIndices();
  }

  // Accessors.
  const HostMemoryAllocator& allocator() const { return allocator_; }
  const Instance& instance() const { return instance_; }
  const PhysicalDevice& physical_device() const { return physical_device_; }
  const QueueFamilyIndices& queue_family_indices() const {
    return physical_device_.queue_family_indices();
  }
  const VkPhysicalDeviceLimits& physical_device_limits() const {
    return physical_device_.physical_device_limits();
  }
  const Device& device() const { return device_; }
  const Queues& queues() const { return queues_; }

 private:
  explicit BasicContext(
      const absl::optional<WindowSupport>& window_support
#ifndef NDEBUG
      , const DebugCallback::TriggerCondition& debug_callback_trigger
#endif /* !NDEBUG */
  )
      : instance_{this, window_support},
#ifndef NDEBUG
        debug_callback_{this, debug_callback_trigger},
#endif /* !NDEBUG */
        physical_device_{this, window_support},
        device_{this, window_support},
        queues_{*this, queue_family_indices()} {}

  // Wrapper of VkAllocationCallbacks.
  const HostMemoryAllocator allocator_;

  // Wrapper of VkInstance.
  const Instance instance_;

#ifndef NDEBUG
  // Wrapper of VkDebugUtilsMessengerEXT.
  const DebugCallback debug_callback_;
#endif /* !NDEBUG */

  // Wrapper of VkPhysicalDevice.
  const PhysicalDevice physical_device_;

  // Wrapper of VkDevice.
  const Device device_;

  // Wrapper of VkQueue.
  const Queues queues_;

  // Ops that are delayed to be executed until the graphics device becomes idle.
  std::vector<ReleaseExpiredResourceOp> release_expired_rsrc_ops_;

  // Ops that are used to check there is no active auto release pool before
  // exiting the program.
  std::vector<std::function<void()>> check_no_active_auto_release_pool_ops_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_CONTEXT_H */
