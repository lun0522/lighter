//
//  basic_object.h
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef VULKAN_WRAPPER_BASIC_OBJECT_H
#define VULKAN_WRAPPER_BASIC_OBJECT_H

#include <vulkan/vulkan.hpp>

#include "util.h"

namespace vulkan {
class Application;  // TODO: move to namespace wrapper
} /* namespace vulkan */

namespace vulkan {
namespace wrapper {

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
  MARK_NOT_COPYABLE_OR_MOVABLE(Instance);

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
  Surface(const Application& app) : app_{app} {}
  void Init();
  ~Surface();
  MARK_NOT_COPYABLE_OR_MOVABLE(Surface);

  VkSurfaceKHR& operator*(void) { return surface_; }
  const VkSurfaceKHR& operator*(void) const { return surface_; }

 private:
  const Application& app_;
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
  PhysicalDevice(Application& app) : app_{app} {}
  void Init();
  ~PhysicalDevice() {}  // implicitly cleaned up
  MARK_NOT_COPYABLE_OR_MOVABLE(PhysicalDevice);

  VkPhysicalDevice& operator*(void) { return physical_device_; }
  const VkPhysicalDevice& operator*(void) const { return physical_device_; }

 private:
  Application& app_;
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
  Device(Application& app) : app_{app} {}
  void Init();
  ~Device() { vkDestroyDevice(device_, nullptr); }
  MARK_NOT_COPYABLE_OR_MOVABLE(Device);

  VkDevice& operator*(void) { return device_; }
  const VkDevice& operator*(void) const { return device_; }

 private:
  Application& app_;
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
  MARK_NOT_COPYABLE_OR_MOVABLE(Queues);
};

} /* namespace wrapper */
} /* namespace vulkan */

#endif /* VULKAN_WRAPPER_BASIC_OBJECT_H */
