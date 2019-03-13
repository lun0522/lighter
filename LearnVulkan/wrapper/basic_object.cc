//
//  basic_object.cc
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "basic_object.h"

#include <iostream>
#include <unordered_set>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "context.h"
#include "swapchain.h"
#include "util.h"
#include "validation.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::runtime_error;
using std::vector;

bool IsDeviceSuitable(Queues& queues,
                      const VkPhysicalDevice& physical_device,
                      const VkSurfaceKHR& surface) {
  // require swap chain support
  if (!Swapchain::HasSwapchainSupport(surface, physical_device)) return false;

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  std::cout << "Found device: " << properties.deviceName
            << std::endl << std::endl;

  VkPhysicalDeviceFeatures features;
  vkGetPhysicalDeviceFeatures(physical_device, &features);

  auto families{util::QueryAttribute<VkQueueFamilyProperties>(
      [&physical_device](uint32_t* count, VkQueueFamilyProperties* properties) {
        return vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, count, properties);
      }
  )};

  // find queue family that holds graphics queue
  auto graphics_support = [](const VkQueueFamilyProperties& family) -> bool {
    return family.queueCount && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
  };
  if (!util::FindFirst<VkQueueFamilyProperties>(
      families, graphics_support, queues.graphics.family_index))
    return false;

  // find queue family that holds present queue
  uint32_t index = 0;
  auto present_support = [&physical_device, &surface, index]
      (const VkQueueFamilyProperties& family) mutable -> bool {
      VkBool32 support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(
          physical_device, index++, surface, &support);
      return support;
  };
  if (!util::FindFirst<VkQueueFamilyProperties>(
      families, present_support, queues.present.family_index))
    return false;

  return true;
}

} /* namespace */

void Instance::Init(std::shared_ptr<Context> context) {
  context_ = context;

  if (glfwVulkanSupported() == GL_FALSE)
    throw runtime_error{"Vulkan not supported"};

  uint32_t glfw_extension_count;
  const char** glfw_extensions =
      glfwGetRequiredInstanceExtensions(&glfw_extension_count);

#ifdef DEBUG
  vector<const char*> required_extensions{
      glfw_extensions,
      glfw_extensions + glfw_extension_count,
  };
  // one extra extension to enable debug report
  required_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  CheckInstanceExtensionSupport({
      required_extensions.begin(),
      required_extensions.end()
  });
  CheckValidationLayerSupport({
      kValidationLayers.begin(),
      kValidationLayers.end()
  });
#endif /* DEBUG */

  // [optional]
  // might be useful for the driver to optimize for some graphics engine
  VkApplicationInfo app_info{
      VK_STRUCTURE_TYPE_APPLICATION_INFO,
      /*pNext=*/nullptr,
      /*pApplicationName=*/"Vulkan Application",
      /*applicationVersion=*/VK_MAKE_VERSION(1, 0, 0),
      /*pEngineName=*/"No Engine",
      /*engineVersion=*/VK_MAKE_VERSION(1, 0, 0),
      /*apiVersion=*/VK_API_VERSION_1_0,
  };

  // [required]
  // tell the driver which global extensions and validation layers to use
  VkInstanceCreateInfo instance_info{
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      &app_info,
#ifdef DEBUG
      // enabled layers
      CONTAINER_SIZE(kValidationLayers),
      kValidationLayers.data(),
      // enabled extensions
      CONTAINER_SIZE(required_extensions),
      required_extensions.data(),
#else
      /*enabledLayerCount=*/0,
      /*ppEnabledLayerNames=*/nullptr,
      // enabled extensions
      glfw_extension_count,
      glfw_extensions,
#endif /* DEBUG */
  };

  ASSERT_SUCCESS(
      vkCreateInstance(&instance_info, context_->allocator(), &instance_),
      "Failed to create instance");
}

Instance::~Instance() {
  vkDestroyInstance(instance_, context_->allocator());
}

void Surface::Init(std::shared_ptr<Context> context) {
  context_ = context;
  ASSERT_SUCCESS(
      glfwCreateWindowSurface(*context->instance(), context->window(),
                              context_->allocator(), &surface_),
      "Failed to create window surface");
}

Surface::~Surface() {
  vkDestroySurfaceKHR(*context_->instance(), surface_, context_->allocator());
}

void PhysicalDevice::Init(std::shared_ptr<Context> context) {
  context_ = context;

  auto devices{util::QueryAttribute<VkPhysicalDevice>(
      [this](uint32_t* count, VkPhysicalDevice* physical_device) {
        return vkEnumeratePhysicalDevices(
            *context_->instance(), count, physical_device);
      }
  )};

  for (const auto& candidate : devices) {
    if (IsDeviceSuitable(context_->queues(), candidate, *context_->surface())) {
      physical_device_ = candidate;
      return;
    }
  }
  throw runtime_error{"Failed to find suitable GPU"};
}

VkPhysicalDeviceLimits PhysicalDevice::limits() const {
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device_, &properties);
  return properties.limits;
}

void Device::Init(std::shared_ptr<Context> context) {
  context_ = context;

  // graphics queue and present queue might be the same
  Queues& queues = context->queues();
  std::unordered_set<uint32_t> queue_families{
      queues.graphics.family_index,
      queues.present.family_index,
  };
  float priority = 1.0f;
  vector<VkDeviceQueueCreateInfo> queue_infos{};
  for (uint32_t queue_family : queue_families) {
    VkDeviceQueueCreateInfo queue_info{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/NULL_FLAG,
        queue_family,
        /*queueCount=*/1,
        &priority,  // always required even if only one queue
    };
    queue_infos.emplace_back(std::move(queue_info));
  }

  VkDeviceCreateInfo device_info{
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      // queue create infos
      CONTAINER_SIZE(queue_infos),
      queue_infos.data(),
#ifdef DEBUG
      // enabled layers
      CONTAINER_SIZE(kValidationLayers),
      kValidationLayers.data(),
#else
      /*enabledLayerCount=*/0,
      /*ppEnabledLayerNames=*/nullptr,
#endif /* DEBUG */
      // enabled extensions
      CONTAINER_SIZE(kSwapChainExtensions),
      kSwapChainExtensions.data(),
      /*pEnabledFeatures=*/nullptr,
  };

  ASSERT_SUCCESS(vkCreateDevice(*context->physical_device(), &device_info,
                                context_->allocator(), &device_),
                 "Failed to create logical device");

  // retrieve queue handles for each queue family
  vkGetDeviceQueue(device_, queues.graphics.family_index, 0,
                   &queues.graphics.queue);
  vkGetDeviceQueue(device_, queues.present.family_index, 0,
                   &queues.present.queue);
}

Device::~Device() {
  vkDestroyDevice(device_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
