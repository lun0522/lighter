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
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class BasicContext;
struct WindowSupport;

/** VkInstance is used to establish connection with Vulkan library and
 *    maintain per-application states.
 *
 *  Initialization:
 *    VkApplicationInfo (App/Engine/API name and version)
 *    Extensions to enable (required by GLFW and debugging)
 *    Layers to enable (required by validation layers)
 */
class Instance {
 public:
  Instance() = default;

  // This class is neither copyable nor movable.
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  ~Instance();

  void Init(std::shared_ptr<BasicContext> context,
            const WindowSupport& window_support);

  const VkInstance& operator*() const { return instance_; }

 private:
  std::shared_ptr<BasicContext> context_;
  VkInstance instance_;
};

/** VkPhysicalDevice is a handle to a physical graphics card. We iterate through
 *    graphics devices to find one that supports swapchains. Then, we iterate
 *    through its queue families to find one family supporting graphics, and
 *    another one supporting presentation (possibly them are identical).
 *    All queues in one family share the same property, so we only need to
 *    find out the index of the family.
 *
 *  Initialization:
 *    VkInstance
 *    VkSurfaceKHR (since we need presentation support)
 */
struct PhysicalDevice {
 public:
  PhysicalDevice() = default;

  // This class is neither copyable nor movable.
  PhysicalDevice(const PhysicalDevice&) = delete;
  PhysicalDevice& operator=(const PhysicalDevice&) = delete;

  // Implicitly cleaned up.
  ~PhysicalDevice() = default;

  void Init(std::shared_ptr<BasicContext> context,
            const WindowSupport& window_support);

  const VkPhysicalDevice& operator*()    const { return physical_device_; }
  const VkPhysicalDeviceLimits& limits() const { return limits_; }

 private:
  std::shared_ptr<BasicContext> context_;
  VkPhysicalDevice physical_device_;
  VkPhysicalDeviceLimits limits_;
};

/** VkDevice interfaces with the physical device. We have to tell Vulkan
 *    how many queues we want to use. Noticed that the graphics queue and
 *    the present queue might be the same queue, we use hash set to remove
 *    duplicated queue family indices.
 *
 *  Initialization:
 *    VkPhysicalDevice
 *    Physical device features to enable
 *    List of VkDeviceQueueCreateInfo (queue family index and how many queues
 *      do we want from this family)
 *    Extensions to enable (required by swapchains)
 *    Layers to enable (required by validation layers)
 */
struct Device {
 public:
  Device() = default;

  // This class is neither copyable nor movable.
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  ~Device();

  void Init(std::shared_ptr<BasicContext> context,
            const WindowSupport& window_support);

  const VkDevice& operator*() const { return device_; }

 private:
  std::shared_ptr<BasicContext> context_;
  VkDevice device_;
};

/** VkQueue is the queue associated with the logical device. When we create it,
 *    we can specify both queue family index and queue index (within family).
 */
struct Queues {
  static constexpr uint32_t kInvalidIndex =
      std::numeric_limits<uint32_t>::max();

  struct Queue {
    Queue() = default;
    VkQueue queue;
    uint32_t family_index;
  };
  Queue graphics, transfer;
  absl::optional<Queue> present;

  Queues() = default;

  // This class is neither copyable nor movable.
  Queues(const Queues&) = delete;
  Queues& operator=(const Queues&) = delete;

  // Implicitly cleaned up with physical device.
  ~Queues() = default;

  absl::flat_hash_set<uint32_t> unique_family_indices() const;
  Queues& set_queues(const VkQueue& graphics_queue,
                     const VkQueue& transfer_queue,
                     const VkQueue* present_queue);
  Queues& set_family_indices(uint32_t graphics_index,
                             uint32_t transfer_index,
                             uint32_t present_index);
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BASIC_OBJECT_H */
