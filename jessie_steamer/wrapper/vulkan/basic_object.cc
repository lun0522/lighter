//
//  basic_object.cc
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/basic_object.h"

#include <iostream>
#include <numeric>
#include <vector>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#ifndef NDEBUG
#include "jessie_steamer/wrapper/vulkan/validation.h"
#endif /* !NDEBUG */

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

namespace util = common::util;

using std::vector;

struct QueueIndices {
  QueueIndices() : graphics{Queues::kInvalidIndex},
                   present{Queues::kInvalidIndex} {}
  uint32_t graphics, present;
};

bool HasSwapchainSupport(const VkPhysicalDevice& physical_device,
                         const WindowSupport& window_support) {
  std::cout << "Checking extension support required for swapchain..."
            << std::endl << std::endl;

  vector<std::string> required{
      window_support.swapchain_extensions.begin(),
      window_support.swapchain_extensions.end(),
  };
  auto extensions{util::QueryAttribute<VkExtensionProperties>(
      [&physical_device](uint32_t* count, VkExtensionProperties* properties) {
        return vkEnumerateDeviceExtensionProperties(
            physical_device, nullptr, count, properties);
      }
  )};
  auto get_name = [](const VkExtensionProperties& property) {
    return property.extensionName;
  };
  auto unsupported = util::FindUnsupported<VkExtensionProperties>(
      required, extensions, get_name);

  if (unsupported.has_value()) {
    std::cout << "Unsupported: " << unsupported.value() << std::endl;
    return false;
  }

  // physical device may support swapchain but maybe not compatible with
  // window system, so we need to query details
  uint32_t format_count, mode_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      physical_device, *window_support.surface, &format_count, nullptr);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      physical_device, *window_support.surface, &mode_count, nullptr);
  return format_count && mode_count;
}

absl::optional<QueueIndices> FindDeviceQueues(
    const VkPhysicalDevice& physical_device,
    const absl::optional<WindowSupport>& window_support) {
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  std::cout << "Found device: " << properties.deviceName
            << std::endl << std::endl;

  // require swapchain support if use window
  if (window_support.has_value() &&
      !HasSwapchainSupport(physical_device, window_support.value())) {
    return absl::nullopt;
  }

  // require anisotropy filtering support
  VkPhysicalDeviceFeatures feature_support;
  vkGetPhysicalDeviceFeatures(physical_device, &feature_support);
  if (!feature_support.samplerAnisotropy) {
    return absl::nullopt;
  }

  QueueIndices candidate;
  auto families{util::QueryAttribute<VkQueueFamilyProperties>(
      [&physical_device](uint32_t* count, VkQueueFamilyProperties* properties) {
        return vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, count, properties);
      }
  )};

  // find queue family that holds graphics queue
  auto graphics_support = [](const VkQueueFamilyProperties& family) {
    return family.queueCount && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
  };
  int graphics_queue_index =
      util::FindFirst<VkQueueFamilyProperties>(families, graphics_support);
  if (graphics_queue_index == util::kInvalidIndex) {
    return absl::nullopt;
  } else {
    candidate.graphics = static_cast<uint32_t>(graphics_queue_index);
  }

  if (window_support.has_value()) {
    // find queue family that holds present queue
    uint32_t index = 0;
    auto present_support = [&physical_device, &window_support, index]
        (const VkQueueFamilyProperties& family) mutable {
      VkBool32 support = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(
          physical_device, index++, *window_support.value().surface, &support);
      return support;
    };
    int present_queue_index =
        util::FindFirst<VkQueueFamilyProperties>(families, present_support);
    if (present_queue_index == util::kInvalidIndex) {
      return absl::nullopt;
    } else {
      candidate.present = static_cast<uint32_t>(present_queue_index);
    }
  }

  return candidate;
}

} /* namespace */

void Instance::Init(SharedBasicContext context,
                    const absl::optional<WindowSupport>& window_support) {
  context_ = std::move(context);

  // request support for pushing descriptors
  vector<const char*> instance_extensions{
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };
  if (window_support.has_value()) {
    instance_extensions.insert(
        instance_extensions.end(),
        window_support.value().window_extensions.begin(),
        window_support.value().window_extensions.end()
    );
  }
#ifndef NDEBUG
  // one extra extension to enable debug report
  instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  validation::EnsureInstanceExtensionSupport({
      instance_extensions.begin(),
      instance_extensions.end()
  });
  validation::EnsureValidationLayerSupport({
      validation::layers().begin(),
      validation::layers().end()
  });
#endif /* !NDEBUG */

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
      /*flags=*/nullflag,
      &app_info,
#ifdef NDEBUG
      /*enabledLayerCount=*/0,
      /*ppEnabledLayerNames=*/nullptr,
#else  /* !NDEBUG */
      // required layers
      CONTAINER_SIZE(validation::layers()),
      validation::layers().data(),
#endif /* NDEBUG */
      CONTAINER_SIZE(instance_extensions),
      instance_extensions.data(),
  };

  ASSERT_SUCCESS(
      vkCreateInstance(&instance_info, context_->allocator(), &instance_),
      "Failed to create instance");
}

Instance::~Instance() {
  vkDestroyInstance(instance_, context_->allocator());
}

void PhysicalDevice::Init(SharedBasicContext context,
                          const absl::optional<WindowSupport>& window_support) {
  context_ = std::move(context);

  auto physical_devices{util::QueryAttribute<VkPhysicalDevice>(
      [this](uint32_t* count, VkPhysicalDevice* physical_device) {
        return vkEnumeratePhysicalDevices(
            *context_->instance(), count, physical_device);
      }
  )};

  for (const auto& candidate : physical_devices) {
    auto indices = FindDeviceQueues(candidate, window_support);
    if (indices.has_value()) {
      physical_device_ = candidate;
      // graphics queue will be used as transfer queue as well
      context_->queues_.set_family_indices(indices.value().graphics,
                                           indices.value().graphics,
                                           indices.value().present);

      // query device limits
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(physical_device_, &properties);
      limits_ = properties.limits;

      return;
    }
  }

  FATAL("Failed to find suitable GPU");
}

void Device::Init(SharedBasicContext context,
                  const absl::optional<WindowSupport>& window_support) {
  context_ = std::move(context);

  // request anisotropy filtering support
  VkPhysicalDeviceFeatures required_features{};
  required_features.samplerAnisotropy = VK_TRUE;

  // request negative-height viewport support
  vector<const char*> device_extensions{
      VK_KHR_MAINTENANCE1_EXTENSION_NAME,
      VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
  };
  if (window_support.has_value()) {
    device_extensions.insert(
        device_extensions.end(),
        window_support.value().swapchain_extensions.begin(),
        window_support.value().swapchain_extensions.end());
  }

  // graphics queue and present queue might be the same
  const Queues& queues = context_->queues();
  float priority = 1.0f;
  vector<VkDeviceQueueCreateInfo> queue_infos;
  for (uint32_t family_index : queues.unique_family_indices()) {
    queue_infos.emplace_back(VkDeviceQueueCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/nullflag,
        family_index,
        /*queueCount=*/1,
        &priority,  // always required even if only one queue
    });
  }

  VkDeviceCreateInfo device_info{
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      // queue create infos
      CONTAINER_SIZE(queue_infos),
      queue_infos.data(),
#ifdef NDEBUG
      /*enabledLayerCount=*/0,
      /*ppEnabledLayerNames=*/nullptr,
#else  /* !NDEBUG */
      // required layers
      CONTAINER_SIZE(validation::layers()),
      validation::layers().data(),
#endif /* NDEBUG */
      CONTAINER_SIZE(device_extensions),
      device_extensions.data(),
      &required_features,
  };

  ASSERT_SUCCESS(vkCreateDevice(*context_->physical_device(), &device_info,
                                context_->allocator(), &device_),
                 "Failed to create logical device");

  // retrieve queue handles for each queue family
  VkQueue graphics_queue, transfer_queue;
  vkGetDeviceQueue(device_, queues.graphics.family_index, 0, &graphics_queue);
  vkGetDeviceQueue(device_, queues.transfer.family_index, 0, &transfer_queue);

  absl::optional<VkQueue> present_queue;
  if (window_support.has_value()) {
    VkQueue queue;
    vkGetDeviceQueue(device_, queues.present.value().family_index, 0, &queue);
    present_queue.emplace(queue);
  }

  context_->queues_.set_queues(graphics_queue, transfer_queue, present_queue);
}

Device::~Device() {
  vkDestroyDevice(device_, context_->allocator());
}

absl::flat_hash_set<uint32_t> Queues::unique_family_indices() const {
  absl::flat_hash_set<uint32_t> indices{graphics.family_index,
                                        transfer.family_index};
  if (present.has_value()) {
    indices.emplace(present.value().family_index);
  }
  return indices;
}

Queues& Queues::set_queues(const VkQueue& graphics_queue,
                           const VkQueue& transfer_queue,
                           const absl::optional<VkQueue>& present_queue) {
  graphics.queue = graphics_queue;
  transfer.queue = transfer_queue;
  if (present.has_value()) {
    if (!present_queue.has_value()) {
      FATAL("Present queue is not specified");
    }
    present.value().queue = present_queue.value();
  } else if (present_queue.has_value()) {
    FATAL("Preset queue should not be specified");
  }
  return *this;
}

Queues& Queues::set_family_indices(uint32_t graphics_index,
                                   uint32_t transfer_index,
                                   uint32_t present_index) {
  graphics.family_index = graphics_index;
  transfer.family_index = transfer_index;
  if (present_index != kInvalidIndex) {
    present = Queue{{}, present_index};
  }
  return *this;
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
