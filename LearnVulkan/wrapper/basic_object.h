//
//  basic_object.h
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef VULKAN_WRAPPER_BASIC_OBJECT_H
#define VULKAN_WRAPPER_BASIC_OBJECT_H

#include <vulkan/vulkan.hpp>

namespace vulkan {
namespace wrapper {

class Context;

/** VkInstance is used to establish connection with Vulkan library and
 *      maintain per-application states.
 *
 *  Initialization:
 *      VkApplicationInfo (App/Engine/API name and version)
 *      Extensions to enable (required by GLFW and debugging)
 *      Layers to enable (required by validation layers)
 */
class Instance {
 public:
  Instance() {}
  void Init();
  ~Instance() { vkDestroyInstance(instance_, nullptr); }

  // This class is neither copyable nor movable
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  VkInstance& operator*(void) { return instance_; }
  const VkInstance& operator*(void) const { return instance_; }

 private:
  VkInstance instance_;
};

/** VkSurfaceKHR interfaces with platform-specific window systems. It is backed
 *      by the window created by GLFW, which hides platform-specific details.
 *      It is not needed for off-screen rendering.
 *
 *  Initialization (by GLFW):
 *      VkInstance
 *      GLFWwindow
 */
class Surface {
 public:
  void Init(std::shared_ptr<Context> context);
  ~Surface();

  // This class is neither copyable nor movable
  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;

  VkSurfaceKHR& operator*(void) { return surface_; }
  const VkSurfaceKHR& operator*(void) const { return surface_; }

 private:
  std::shared_ptr<Context> context_;
  VkSurfaceKHR surface_;
};

/** VkPhysicalDevice is a handle to a physical graphics card. We iterate through
 *      graphics devices to find one that supports swap chains. Then, we iterate
 *      through its queue families to find one family supporting graphics, and
 *      another one supporting presentation (possibly them are identical).
 *      All queues in one family share the same property, so we only need to
 *      find out the index of the family.
 *
 *  Initialization:
 *      VkInstance
 *      VkSurfaceKHR (since we need presentation support)
 */
struct PhysicalDevice {
 public:
  void Init(std::shared_ptr<Context> context);
  ~PhysicalDevice() {}  // implicitly cleaned up

  // This class is neither copyable nor movable
  PhysicalDevice(const PhysicalDevice&) = delete;
  PhysicalDevice& operator=(const PhysicalDevice&) = delete;

  VkPhysicalDevice& operator*(void) { return physical_device_; }
  const VkPhysicalDevice& operator*(void) const { return physical_device_; }
  VkPhysicalDeviceLimits limits() const;

 private:
  std::shared_ptr<Context> context_;
  VkPhysicalDevice physical_device_;
};

/** VkDevice interfaces with the physical device. We have to tell Vulkan
 *      how many queues we want to use. Noticed that the graphics queue and
 *      the present queue might be the same queue, we use hash set to remove
 *      duplicated queue family indices.
 *
 *  Initialization:
 *      VkPhysicalDevice
 *      Physical device features to enable
 *      List of VkDeviceQueueCreateInfo (queue family index and how many
 *          queues do we want from this family)
 *      Extensions to enable (required by swap chains)
 *      Layers to enable (required by validation layers)
 */
struct Device {
 public:
  void Init(std::shared_ptr<Context> context);
  ~Device() { vkDestroyDevice(device_, nullptr); }

  // This class is neither copyable nor movable
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  VkDevice& operator*(void) { return device_; }
  const VkDevice& operator*(void) const { return device_; }

 private:
  std::shared_ptr<Context> context_;
  VkDevice device_;
};

/** VkQueue is the queue associated with the logical device. When we create it,
 *      we can specify both queue family index and queue index (within family).
 */
struct Queues {
  struct Queue {
    VkQueue queue;
    uint32_t family_index;
  };
  Queue graphics, present;
  Queues() = default;
  ~Queues() {}  // implicitly cleaned up with physical device

  // This class is neither copyable nor movable
  Queues(const Queues&) = delete;
  Queues& operator=(const Queues&) = delete;
};

} /* namespace wrapper */
} /* namespace vulkan */

#endif /* VULKAN_WRAPPER_BASIC_OBJECT_H */
