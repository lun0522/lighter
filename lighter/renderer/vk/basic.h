//
//  basic.h
//
//  Created by Pujun Lun on 10/22/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_BASIC_H
#define LIGHTER_RENDERER_VK_BASIC_H

#include <vector>

#include "lighter/common/window.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {

// Forward declarations.
class Context;

// Wraps VkAllocationCallbacks, which is used for allocating space on the host
// for Vulkan objects. For now this wrapper class simply does nothing.
class HostMemoryAllocator {
 public:
  HostMemoryAllocator() = default;

  // This class is neither copyable nor movable.
  HostMemoryAllocator(const HostMemoryAllocator&) = delete;
  HostMemoryAllocator& operator=(const HostMemoryAllocator&) = delete;

  // Overloads.
  const VkAllocationCallbacks* operator*() const {
    return allocation_callback_;
  }

 private:
  // Used to allocate memory on the host.
  const VkAllocationCallbacks* allocation_callback_ = nullptr;
};

// Wraps VkInstance, which is used to establish connection with Vulkan library
// and maintain per-application states.
class Instance {
 public:
  Instance(const Context* context, absl::string_view application_name,
           absl::Span<const common::Window* const> windows);

  // This class is neither copyable nor movable.
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  ~Instance();

  // Overloads.
  const VkInstance& operator*() const { return instance_; }

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque instance object.
  VkInstance instance_;
};

// Wraps VkSurfaceKHR, which interfaces with platform-specific window systems.
class Surface {
 public:
  Surface(const Context* context, const common::Window& window);

  // This class is neither copyable nor movable.
  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;

  ~Surface();

  // Returns the capabilities of this surface.
  VkSurfaceCapabilitiesKHR GetCapabilities() const;

  // Overloads.
  const VkSurfaceKHR& operator*() const { return surface_; }

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque surface object.
  VkSurfaceKHR surface_;
};

// Wraps VkPhysicalDevice, which is the handle to physical graphics card.
struct PhysicalDevice {
 public:
  // Holds queue family indices for the queues we need.
  // All queues in one family share the same property.
  struct QueueFamilyIndices {
    uint32_t graphics;
    uint32_t compute;
    std::vector<uint32_t> presents;
  };

  PhysicalDevice(const Context* context,
                 absl::Span<const Surface* const> surfaces,
                 absl::Span<const char* const> swapchain_extensions);

  // This class is neither copyable nor movable.
  PhysicalDevice(const PhysicalDevice&) = delete;
  PhysicalDevice& operator=(const PhysicalDevice&) = delete;

  // Implicitly cleaned up.
  ~PhysicalDevice() = default;

  // Returns unique queue family indices. Note that we might be using the same
  // queue family for different purposes.
  absl::flat_hash_set<uint32_t> GetUniqueFamilyIndices() const;

  // Overloads.
  const VkPhysicalDevice& operator*() const { return physical_device_; }

  // Accessors.
  const QueueFamilyIndices& queue_family_indices() const {
    return queue_family_indices_;
  }
  const VkPhysicalDeviceLimits& physical_device_limits() const {
    return physical_device_limits_;
  }

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque physical device object.
  VkPhysicalDevice physical_device_;

  // Family indices for the queues we need.
  QueueFamilyIndices queue_family_indices_;

  // Limits of the physical device.
  VkPhysicalDeviceLimits physical_device_limits_;
};

// Wraps VkDevice, which interfaces with the physical device.
struct Device {
 public:
  Device(const Context* context,
         absl::Span<const char* const> swapchain_extensions);

  // This class is neither copyable nor movable.
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  ~Device();

  // Blocks host until 'device_' becomes idle.
  void WaitIdle() const { vkDeviceWaitIdle(device_); }

  // Overloads.
  const VkDevice& operator*() const { return device_; }

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque device object.
  VkDevice device_;
};

// Wraps VkQueue, which is the queue associated with the logical device.
class Queues {
 public:
  // Holds an opaque queue object and its family index.
  struct Queue {
    VkQueue queue;
    uint32_t family_index;
  };

  explicit Queues(const Context& context);

  // This class is neither copyable nor movable.
  Queues(const Queues&) = delete;
  Queues& operator=(const Queues&) = delete;

  // Implicitly cleaned up with physical device.
  ~Queues() = default;

  // Accessors.
  const Queue& graphics_queue() const { return graphics_queue_; }
  const Queue& compute_queue() const { return compute_queue_; }
  const Queue& present_queue(int window_index) const {
    return present_queues_.at(window_index);
  }

 private:
  // Graphics queue.
  Queue graphics_queue_;

  // Compute queue.
  Queue compute_queue_;

  // Presentation queues. We have one such queue for each window.
  std::vector<Queue> present_queues_;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_BASIC_H */
