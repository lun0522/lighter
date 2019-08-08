//
//  basic_object.h
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_OBJECT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_OBJECT_H

#include <memory>
#include <numeric>

#include "absl/container/flat_hash_set.h"
#include "absl/types/optional.h"
#include "jessie_steamer/common/util.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// Forward declarations.
class BasicContext;
struct WindowSupport;

// VkQueue is the queue associated with the logical device.
class Queues {
 public:
  // Holds queue family indices for the queues we need.
  // All queues in one family share the same property.
  struct FamilyIndices {
    uint32_t graphics;
    uint32_t transfer;
    absl::optional<uint32_t> present;
  };

  // Holds an opaque queue object and its family index.
  struct Queue {
    Queue() = default;

    VkQueue queue;
    uint32_t family_index;
  };

  Queues(const VkDevice& device, const FamilyIndices& family_indices);

  // This class is neither copyable nor movable.
  Queues(const Queues&) = delete;
  Queues& operator=(const Queues&) = delete;

  // Implicitly cleaned up with physical device.
  ~Queues() = default;

  // Accessors.
  const Queue& graphics_queue() const { return graphics_queue_; }
  const Queue& transfer_queue() const { return transfer_queue_; }
  const Queue& present_queue() const {
    ASSERT_HAS_VALUE(present_queue_, "No presentation queue");
    return present_queue_.value();
  }
  const absl::flat_hash_set<uint32_t>& unique_family_indices() const {
    return unique_family_indices_;
  }

 private:
  // Populates 'queue' with the first queue in the family with 'family_index'.
  void SetQueue(const VkDevice& device, uint32_t family_index,
                Queue* queue) const;

  // The graphics queue.
  Queue graphics_queue_;

  // The transfer queue.
  Queue transfer_queue_;

  // The presentation queue.
  absl::optional<Queue> present_queue_;

  // Holds unique queue family indices.
  const absl::flat_hash_set<uint32_t> unique_family_indices_;
};

// VkInstance is used to establish connection with Vulkan library and maintain
// per-application states.
class Instance {
 public:
  Instance() = default;

  // This class is neither copyable nor movable.
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  ~Instance();

  // Initializes 'instance_'.
  void Init(std::shared_ptr<BasicContext> context,
            const absl::optional<WindowSupport>& window_support);

  // Overloads.
  const VkInstance& operator*() const { return instance_; }

 private:
  // Pointer to context.
  std::shared_ptr<BasicContext> context_;

  // Opaque instance object.
  VkInstance instance_;
};

// VkPhysicalDevice is a handle to a physical graphics card.
struct PhysicalDevice {
 public:
  PhysicalDevice() = default;

  // This class is neither copyable nor movable.
  PhysicalDevice(const PhysicalDevice&) = delete;
  PhysicalDevice& operator=(const PhysicalDevice&) = delete;

  // Implicitly cleaned up.
  ~PhysicalDevice() = default;

  // Finds a physical device that has the queues we need.
  // If there is such a device, the limits of it will be queried and stored
  // in 'limits_', and the family indices of queues will be returned.
  // Otherwise, throws a runtime exception.
  Queues::FamilyIndices Init(
      std::shared_ptr<BasicContext> context,
      const absl::optional<WindowSupport>& window_support);

  // Overloads.
  const VkPhysicalDevice& operator*() const { return physical_device_; }

  // Accessors.
  const VkPhysicalDeviceLimits& limits() const { return limits_; }

 private:
  // Pointer to context.
  std::shared_ptr<BasicContext> context_;

  // Opaque physical device object.
  VkPhysicalDevice physical_device_;

  // Limits of the physical device.
  VkPhysicalDeviceLimits limits_;
};

// VkDevice interfaces with the physical device.
struct Device {
 public:
  Device() = default;

  // This class is neither copyable nor movable.
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  ~Device();

  // Initializes 'device_' and returns the queues retrieved from it.
  std::unique_ptr<Queues> Init(
      std::shared_ptr<BasicContext> context,
      const Queues::FamilyIndices& queue_family_indices,
      const absl::optional<WindowSupport>& window_support);

  // Blocks host until 'device_' becomes idle.
  void WaitIdle() const { vkDeviceWaitIdle(device_); }

  // Overloads.
  const VkDevice& operator*() const { return device_; }

 private:
  // Pointer to context.
  std::shared_ptr<BasicContext> context_;

  // Opaque device object.
  VkDevice device_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_OBJECT_H */
