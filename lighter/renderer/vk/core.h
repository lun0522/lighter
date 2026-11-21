#ifndef LIGHTER_RENDERER_VK_CORE_H
#define LIGHTER_RENDERER_VK_CORE_H

#include <optional>
#include <span>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "lighter/base/window.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/util.h"

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
    return nullptr;
  }

  // Accessors.
  const VkAllocationCallbacks* c_type() const { return nullptr; }
};

// Wraps VkInstance, which is used to establish connection with Vulkan library
// and maintain per-application states.
class Instance {
 public:
  Instance(const Context* context, bool enable_validation,
           const char* application_name,
           std::span<const base::Window* const> windows);

  // This class is neither copyable nor movable.
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  ~Instance() = default;

  // Overloads.
  intl::Instance operator*() const { return *instance_; }
  const intl::Instance* operator->() const { return &(*instance_); }

  // Accessors.
  const intl::raii::Instance& handle() const { return instance_; }

 private:
  // Loader context and instance must remain in this declaration order so the
  // instance is destroyed before the dynamic loader context.
  intl::raii::Context loader_context_;
  intl::raii::Instance instance_{nullptr};
};

// Wraps VkDebugUtilsMessengerEXT, which relays debug messages from the graphics
// driver back to application.
class DebugMessenger {
 public:
  DebugMessenger(const Context* context,
                 const ir::debug_message::Config& config);

  // This class is neither copyable nor movable.
  DebugMessenger(const DebugMessenger&) = delete;
  DebugMessenger& operator=(const DebugMessenger&) = delete;

  ~DebugMessenger() = default;

 private:
  // Opaque debug messenger object.
  intl::raii::DebugUtilsMessengerEXT messenger_{nullptr};
};

// Wraps VkSurfaceKHR, which interfaces with platform-specific window systems.
class Surface {
 public:
  Surface(const Context* context, const base::Window& window);

  // This class is neither copyable nor movable.
  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;

  ~Surface() = default;

  // Overloads.
  intl::SurfaceKHR operator*() const { return *surface_; }

  // Modifiers.
  void SetCapabilities(intl::SurfaceCapabilitiesKHR&& capabilities) {
    capabilities_ = std::move(capabilities);
  }

  // Accessors.
  const intl::SurfaceCapabilitiesKHR& capabilities() const {
    return capabilities_.value();
  }

 private:
  // Opaque surface object.
  intl::raii::SurfaceKHR surface_{nullptr};

  // Capabilities of this surface.
  std::optional<intl::SurfaceCapabilitiesKHR> capabilities_;
};

// Wraps VkPhysicalDevice, which is the handle to physical graphics card.
class PhysicalDevice {
 public:
  // Holds queue family indices for the queues we need.
  // All queues in one family share the same property.
  struct QueueFamilyIndices {
    uint32_t graphics;
    uint32_t compute;
    std::vector<uint32_t> presents;
  };

  PhysicalDevice(const Context* context, std::span<Surface* const> surfaces);

  // This class is neither copyable nor movable.
  PhysicalDevice(const PhysicalDevice&) = delete;
  PhysicalDevice& operator=(const PhysicalDevice&) = delete;

  // Implicitly cleaned up.
  ~PhysicalDevice() = default;

  // Overloads.
  intl::PhysicalDevice operator*() const { return *physical_device_; }
  const intl::PhysicalDevice* operator->() const {
    return &(*physical_device_);
  }

  // Accessors.
  const intl::raii::PhysicalDevice& handle() const { return physical_device_; }
  const QueueFamilyIndices& queue_family_indices() const {
    return queue_family_indices_;
  }
  const intl::PhysicalDeviceLimits& limits() const { return limits_; }
  intl::SampleCountFlagBits sample_count(ir::MultisamplingMode mode) const {
    return sample_count_map_.at(mode);
  }

 private:
  // Opaque physical device object.
  intl::raii::PhysicalDevice physical_device_{nullptr};

  // Family indices for the queues we need.
  QueueFamilyIndices queue_family_indices_;

  // Limits of the physical device.
  intl::PhysicalDeviceLimits limits_;

  // Maps multisampling modes to the sample count that should be chosen.
  std::unordered_map<ir::MultisamplingMode, intl::SampleCountFlagBits>
      sample_count_map_;
};

// Wraps VkDevice, which interfaces with the physical device.
class Device {
 public:
  Device(const Context* context, bool enable_swapchain);

  // This class is neither copyable nor movable.
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  ~Device() = default;

  // Blocks host until the device becomes idle.
  void WaitIdle() const { device_.waitIdle(); }

  // Overloads.
  intl::Device operator*() const { return *device_; }
  const intl::Device* operator->() const { return &(*device_); }

  // Accessors.
  const intl::raii::Device& handle() const { return device_; }

 private:
  // Opaque device object.
  intl::raii::Device device_{nullptr};
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

#endif  // LIGHTER_RENDERER_VK_CORE_H
