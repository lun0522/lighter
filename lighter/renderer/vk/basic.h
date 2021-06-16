//
//  basic.h
//
//  Created by Pujun Lun on 10/22/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_BASIC_H
#define LIGHTER_RENDERER_VK_BASIC_H

#include <optional>
#include <vector>

#include "lighter/common/window.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk {

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
  intl::Optional<const intl::AllocationCallbacks> operator*() const {
    return {&allocation_callbacks_};
  }

  // Accessors.
  VkAllocationCallbacks handle() const {
    return static_cast<VkAllocationCallbacks>(allocation_callbacks_);
  }

 private:
  // Used to allocate memory on the host.
  intl::AllocationCallbacks allocation_callbacks_;
};

// Wraps VkInstance, which is used to establish connection with Vulkan library
// and maintain per-application states.
class Instance {
 public:
  Instance(const Context* context, bool enable_validation,
           const char* application_name,
           absl::Span<const common::Window* const> windows);

  // This class is neither copyable nor movable.
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  ~Instance();

  // Overloads.
  intl::Instance operator*() const { return instance_; }
  const intl::Instance* operator->() const { return &instance_; }

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque instance object.
  intl::Instance instance_;
};

// Wraps VkDebugUtilsMessengerEXT, which relays debug messages from the graphics
// driver back to application.
class DebugMessenger {
 public:
  DebugMessenger(const Context* context, const debug_message::Config& config);

  // This class is neither copyable nor movable.
  DebugMessenger(const DebugMessenger&) = delete;
  DebugMessenger& operator=(const DebugMessenger&) = delete;

  ~DebugMessenger();

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque debug messenger object.
  intl::DebugUtilsMessengerEXT messenger_;
};

// Wraps VkSurfaceKHR, which interfaces with platform-specific window systems.
class Surface {
 public:
  Surface(const Context* context, const common::Window& window);

  // This class is neither copyable nor movable.
  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;

  ~Surface();

  // Overloads.
  intl::SurfaceKHR operator*() const { return surface_; }

  // Modifiers.
  void SetCapabilities(intl::SurfaceCapabilitiesKHR&& capabilities) {
    capabilities_ = std::move(capabilities);
  }

  // Accessors.
  const intl::SurfaceCapabilitiesKHR& capabilities() const {
    return capabilities_.value();
  }

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque surface object.
  intl::SurfaceKHR surface_;

  // Capabilities of this surface.
  std::optional<intl::SurfaceCapabilitiesKHR> capabilities_;
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

  PhysicalDevice(const Context* context, absl::Span<Surface* const> surfaces);

  // This class is neither copyable nor movable.
  PhysicalDevice(const PhysicalDevice&) = delete;
  PhysicalDevice& operator=(const PhysicalDevice&) = delete;

  // Implicitly cleaned up.
  ~PhysicalDevice() = default;

  // Overloads.
  intl::PhysicalDevice operator*() const { return physical_device_; }
  const intl::PhysicalDevice* operator->() const { return &physical_device_; }

  // Accessors.
  const QueueFamilyIndices& queue_family_indices() const {
    return queue_family_indices_;
  }
  const intl::PhysicalDeviceLimits& limits() const { return limits_; }
  intl::SampleCountFlagBits sample_count(MultisamplingMode mode) const {
    return sample_count_map_.at(mode);
  }

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque physical device object.
  intl::PhysicalDevice physical_device_;

  // Family indices for the queues we need.
  QueueFamilyIndices queue_family_indices_;

  // Limits of the physical device.
  intl::PhysicalDeviceLimits limits_;

  // Maps multisampling modes to the sample count that should be chosen.
  absl::flat_hash_map<MultisamplingMode, intl::SampleCountFlagBits>
      sample_count_map_;
};

// Wraps VkDevice, which interfaces with the physical device.
struct Device {
 public:
  Device(const Context* context, bool enable_validation, bool enable_swapchain);

  // This class is neither copyable nor movable.
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  ~Device();

  // Blocks host until 'device_' becomes idle.
  void WaitIdle() const { device_.waitIdle(); }

  // Overloads.
  intl::Device operator*() const { return device_; }
  const intl::Device* operator->() const { return &device_; }

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque device object.
  intl::Device device_;
};

// Wraps VkQueue, which is the queue associated with the logical device.
class Queues {
 public:
  explicit Queues(const Context& context);

  // This class is neither copyable nor movable.
  Queues(const Queues&) = delete;
  Queues& operator=(const Queues&) = delete;

  // Implicitly cleaned up with physical device.
  ~Queues() = default;

  // Accessors.
  intl::Queue graphics_queue() const { return graphics_queue_; }
  intl::Queue compute_queue() const { return compute_queue_; }
  intl::Queue present_queue(int window_index) const {
    return present_queues_.at(window_index);
  }

 private:
  // Graphics queue.
  intl::Queue graphics_queue_;

  // Compute queue.
  intl::Queue compute_queue_;

  // Presentation queues. We have one such queue for each window.
  std::vector<intl::Queue> present_queues_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_BASIC_H
