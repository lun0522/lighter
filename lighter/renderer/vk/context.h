//
//  context.h
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_CONTEXT_H
#define LIGHTER_RENDERER_VK_CONTEXT_H

#include <memory>
#include <string>
#include <vector>

#include "lighter/common/window.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/basic.h"
#include "lighter/renderer/vk/debug_callback.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {
namespace vk {

// Forward declarations.
class Context;

using SharedContext = std::shared_ptr<Context>;

class Context : public std::enable_shared_from_this<Context> {
 public:
  static SharedContext CreateContext(
      absl::string_view application_name,
      const absl::optional<debug_message::Config>& debug_message_config,
      absl::Span<const common::Window* const> windows,
      absl::Span<const char* const> swapchain_extensions) {
    return std::shared_ptr<Context>(
        new Context{application_name, debug_message_config, windows,
                    swapchain_extensions});
  }

  // This class is neither copyable nor movable.
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

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
  Context(absl::string_view application_name,
          const absl::optional<debug_message::Config>& debug_message_config,
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
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_CONTEXT_H */
