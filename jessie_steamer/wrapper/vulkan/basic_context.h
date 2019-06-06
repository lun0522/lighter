//
//  basic_context.h
//
//  Created by Pujun Lun on 3/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_CONTEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_CONTEXT_H

#include <memory>
#include <string>
#include <vector>

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

struct WindowSupport {
  bool is_required;
  const VkSurfaceKHR* surface;
  std::vector<const char*> window_extensions;
  std::vector<const char*> swapchain_extensions;
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

  void Init(const WindowSupport& window_support,
            const VkAllocationCallbacks* allocator) {
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
    physical_device_.Init(ptr(), window_support);
    device_.Init(ptr(), window_support);
  }

  void WaitIdle() const { vkDeviceWaitIdle(*device_); }

  SharedBasicContext ptr()                        { return shared_from_this(); }
  const VkAllocationCallbacks* allocator()  const { return allocator_; }
  const Instance& instance()                const { return instance_; }
  const PhysicalDevice& physical_device()   const { return physical_device_; }
  const Device& device()                    const { return device_; }
  const Queues& queues()                    const { return queues_; }

 private:
  // These methods will set queues.
  friend void PhysicalDevice::Init(SharedBasicContext, const WindowSupport&);
  friend void Device::Init(SharedBasicContext, const WindowSupport&);

  const VkAllocationCallbacks* allocator_ = nullptr;
  Instance instance_;
  PhysicalDevice physical_device_;
  Device device_;
  Queues queues_;
#ifndef NDEBUG
  DebugCallback debug_callback_;
#endif /* !NDEBUG */
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_CONTEXT_H */
