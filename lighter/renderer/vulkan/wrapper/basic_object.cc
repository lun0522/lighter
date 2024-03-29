//
//  basic_object.cc
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/basic_object.h"

#include <functional>
#include <vector>

#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#ifndef NDEBUG
#include "lighter/renderer/vulkan/wrapper/validation.h"
#endif /* !NDEBUG */
#include "third_party/absl/strings/str_cat.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

// Checks support for 'required' extensions, and throws a runtime exception
// if any of them is not supported.
void CheckInstanceExtensionSupport(const std::vector<std::string>& required) {
  LOG_INFO << "Checking instance extension support...";
  LOG_INFO;

  const auto properties = util::QueryAttribute<VkExtensionProperties>(
      [](uint32_t* count, VkExtensionProperties* properties) {
        return vkEnumerateInstanceExtensionProperties(
            /*pLayerName=*/nullptr, count, properties);
      }
  );
  const auto get_name = [](const VkExtensionProperties& property) {
    return property.extensionName;
  };
  const auto unsupported = util::FindUnsupported<VkExtensionProperties>(
      required, properties, get_name);
  ASSERT_NO_VALUE(unsupported,
                  absl::StrCat("Unsupported: ", unsupported.value()));
}

#ifndef NDEBUG
// Checks support for 'required' layers, and throws a runtime exception if any
// of them is not supported.
void CheckValidationLayerSupport(const std::vector<std::string>& required) {
  LOG_INFO << "Checking validation layer support...";
  LOG_INFO;

  const auto properties = util::QueryAttribute<VkLayerProperties>(
      [](uint32_t* count, VkLayerProperties* properties) {
        return vkEnumerateInstanceLayerProperties(count, properties);
      }
  );
  const auto get_name = [](const VkLayerProperties& property) {
    return property.layerName;
  };
  const auto unsupported = util::FindUnsupported<VkLayerProperties>(
      required, properties, get_name);
  ASSERT_NO_VALUE(unsupported,
                  absl::StrCat("Unsupported: ", unsupported.value()));
}
#endif /* !NDEBUG */

// Returns whether swapchain is supported.
bool HasSwapchainSupport(const VkPhysicalDevice& physical_device,
                         const WindowSupport& window_support) {
  LOG_INFO << "Checking swapchain support...";
  LOG_INFO;

  // Query support for device extensions.
  const std::vector<std::string> required{
      window_support.swapchain_extensions.begin(),
      window_support.swapchain_extensions.end(),
  };
  const auto extensions = util::QueryAttribute<VkExtensionProperties>(
      [&physical_device](uint32_t* count, VkExtensionProperties* properties) {
        return vkEnumerateDeviceExtensionProperties(
            physical_device, /*pLayerName=*/nullptr, count, properties);
      }
  );
  const auto get_name = [](const VkExtensionProperties& property) {
    return property.extensionName;
  };
  const auto unsupported = util::FindUnsupported<VkExtensionProperties>(
      required, extensions, get_name);
  if (unsupported.has_value()) {
    LOG_INFO << "Unsupported: " << unsupported.value();
    return false;
  }

  // Physical device may support swapchain but maybe not compatible with the
  // window system, so we need to query details.
  uint32_t format_count, mode_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      physical_device, window_support.surface, &format_count,
      /*pSurfaceFormats=*/nullptr);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      physical_device, window_support.surface, &mode_count,
      /*pPresentModes=*/nullptr);
  return format_count && mode_count;
}

// Finds family indices of queues we need. If any queue is not found in the
// given 'physical_device', returns std::nullopt.
// The graphics queue will also be used as transfer queue.
std::optional<QueueFamilyIndices> FindDeviceQueues(
    const VkPhysicalDevice& physical_device,
    const std::optional<WindowSupport>& window_support) {
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  LOG_INFO << "Found device: " << properties.deviceName;
  LOG_INFO;

  // Request swapchain support if use window.
  if (window_support.has_value() &&
      !HasSwapchainSupport(physical_device, window_support.value())) {
    return std::nullopt;
  }

  // Request support for anisotropy filtering.
  VkPhysicalDeviceFeatures feature_support;
  vkGetPhysicalDeviceFeatures(physical_device, &feature_support);
  if (!feature_support.samplerAnisotropy) {
    return std::nullopt;
  }

  // Find queue family that holds graphics queue.
  QueueFamilyIndices candidate{};
  const auto families = util::QueryAttribute<VkQueueFamilyProperties>(
      [&physical_device](uint32_t* count, VkQueueFamilyProperties* properties) {
        return vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, count, properties);
      }
  );

  const auto has_graphics_support = [](const VkQueueFamilyProperties& family) {
    return family.queueCount && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
  };
  const auto graphics_queue_index =
      common::util::FindIndexOfFirstIf<VkQueueFamilyProperties>(
          families, has_graphics_support);
  if (!graphics_queue_index.has_value()) {
    return std::nullopt;
  } else {
    candidate.graphics = candidate.transfer =
        static_cast<uint32_t>(graphics_queue_index.value());
  }

  const auto has_compute_support = [](const VkQueueFamilyProperties& family) {
    return family.queueCount && (family.queueFlags & VK_QUEUE_COMPUTE_BIT);
  };
  const auto compute_queue_index =
      common::util::FindIndexOfFirstIf<VkQueueFamilyProperties>(
          families, has_compute_support);
  if (!compute_queue_index.has_value()) {
    return std::nullopt;
  } else {
    candidate.compute = static_cast<uint32_t>(compute_queue_index.value());
  }

  // Find queue family that holds presentation queue if use window.
  if (window_support.has_value()) {
    uint32_t index = 0;
    const auto has_present_support = [&physical_device, &window_support, index]
        (const VkQueueFamilyProperties& family) mutable {
      VkBool32 support = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(
          physical_device, index++, window_support->surface, &support);
      return support;
    };
    const auto iter = std::find_if(families.begin(), families.end(),
                                   has_present_support);
    if (iter == families.end()) {
      return std::nullopt;
    } else {
      candidate.present =
          static_cast<uint32_t>(std::distance(families.begin(), iter));
    }
  }

  return candidate;
}

} /* namespace */

std::vector<uint32_t> QueueFamilyIndices::GetUniqueFamilyIndices() const {
  std::vector<uint32_t> queue_family_indices{graphics, compute, transfer};
  if (present.has_value()) {
    queue_family_indices.push_back(present.value());
  }
  common::util::RemoveDuplicate(queue_family_indices);
  return queue_family_indices;
}

Instance::Instance(const BasicContext* context,
                   const std::optional<WindowSupport>& window_support)
    : context_{FATAL_IF_NULL(context)} {
  // Request support for pushing descriptors.
  std::vector<const char*> instance_extensions{
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };
  // Request support for window if necessary.
  if (window_support.has_value()) {
    instance_extensions.insert(
        instance_extensions.end(),
        window_support->window_extensions.begin(),
        window_support->window_extensions.end()
    );
  }
#ifndef NDEBUG
  // Request support for debug reports.
  instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif /* !NDEBUG */

  // Make sure we have support for relevant extensions and layers.
  CheckInstanceExtensionSupport({
      instance_extensions.begin(),
      instance_extensions.end()
  });
#ifndef NDEBUG
  CheckValidationLayerSupport({
      validation::GetRequiredLayers().begin(),
      validation::GetRequiredLayers().end()
  });
#endif /* !NDEBUG */

  // [optional]
  // Might be useful for the driver to optimize for some graphics engine.
  const VkApplicationInfo app_info{
      VK_STRUCTURE_TYPE_APPLICATION_INFO,
      /*pNext=*/nullptr,
      /*pApplicationName=*/"Vulkan Application",
      /*applicationVersion=*/VK_MAKE_VERSION(1, 0, 0),
      /*pEngineName=*/"Lighter",
      /*engineVersion=*/VK_MAKE_VERSION(1, 0, 0),
      VK_API_VERSION_1_0,
  };

  // [required]
  // Specify which global extensions and validation layers to use.
  const VkInstanceCreateInfo instance_info{
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      &app_info,
#ifdef NDEBUG
      /*enabledLayerCount=*/0,
      /*ppEnabledLayerNames=*/nullptr,
#else  /* !NDEBUG */
      CONTAINER_SIZE(validation::GetRequiredLayers()),
      validation::GetRequiredLayers().data(),
#endif /* NDEBUG */
      CONTAINER_SIZE(instance_extensions),
      instance_extensions.data(),
  };

  ASSERT_SUCCESS(
      vkCreateInstance(&instance_info, *context_->allocator(), &instance_),
      "Failed to create instance");

  // Create surface if the window support is requested.
  if (window_support.has_value()) {
    window_support->create_surface(context_);
  }
}

Instance::~Instance() {
  vkDestroyInstance(instance_, *context_->allocator());
}

PhysicalDevice::PhysicalDevice(
    const BasicContext* context,
    const std::optional<WindowSupport>& window_support)
    : context_{FATAL_IF_NULL(context)}, physical_device_{VK_NULL_HANDLE} {
  // Find all physical devices.
  const auto physical_devices = util::QueryAttribute<VkPhysicalDevice>(
      [this](uint32_t* count, VkPhysicalDevice* physical_device) {
        return vkEnumeratePhysicalDevices(
            *context_->instance(), count, physical_device);
      }
  );

  // Find a suitable device. If window support is requested, also request for
  // the swapchain and presentation queue here.
  for (const auto& candidate : physical_devices) {
    const auto indices = FindDeviceQueues(candidate, window_support);
    if (indices.has_value()) {
      physical_device_ = candidate;
      queue_family_indices_ = indices.value();

      // Query physical device limits.
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(physical_device_, &properties);
      physical_device_limits_ = properties.limits;

      // Prefer discrete GPUs.
      if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        LOG_INFO << "Use this discrete GPU";
        return;
      } else {
        LOG_INFO << "This not a discrete GPU, keep searching";
      }
    }
  }

  ASSERT_FALSE(physical_device_ == VK_NULL_HANDLE,
               "Failed to find suitable graphics device");
  LOG_INFO << "Use the previous found GPU";
}

Device::Device(const BasicContext* context,
               const std::optional<WindowSupport>& window_support)
    : context_{FATAL_IF_NULL(context)} {
  if (window_support.has_value()) {
    ASSERT_HAS_VALUE(context_->queue_family_indices().present,
                     "Presentation queue is not properly set up");
  }

  // Request support for anisotropy filtering.
  VkPhysicalDeviceFeatures required_features{};
  required_features.samplerAnisotropy = VK_TRUE;

  // Request support for negative-height viewport and pushing descriptors.
  std::vector<const char*> device_extensions{
      VK_KHR_MAINTENANCE1_EXTENSION_NAME,
      VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
  };
  // Request support for window if necessary.
  if (window_support.has_value()) {
    device_extensions.insert(
        device_extensions.end(),
        window_support->swapchain_extensions.begin(),
        window_support->swapchain_extensions.end());
  }

  // Specify which queue we want to use.
  std::vector<VkDeviceQueueCreateInfo> queue_infos;
  // 'priority' is always required even if there is only one queue.
  constexpr float kPriority = 1.0f;
  for (uint32_t family_index : context_->GetUniqueFamilyIndices()) {
    queue_infos.push_back(VkDeviceQueueCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/nullflag,
        family_index,
        /*queueCount=*/1,
        &kPriority,
    });
  }

  const VkDeviceCreateInfo device_info{
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(queue_infos),
      queue_infos.data(),
#ifdef NDEBUG
      /*enabledLayerCount=*/0,
      /*ppEnabledLayerNames=*/nullptr,
#else  /* !NDEBUG */
      CONTAINER_SIZE(validation::GetRequiredLayers()),
      validation::GetRequiredLayers().data(),
#endif /* NDEBUG */
      CONTAINER_SIZE(device_extensions),
      device_extensions.data(),
      &required_features,
  };

  ASSERT_SUCCESS(vkCreateDevice(*context_->physical_device(), &device_info,
                                *context_->allocator(), &device_),
                 "Failed to create logical device");
}

Device::~Device() {
  vkDestroyDevice(device_, *context_->allocator());
}

Queues::Queues(const BasicContext& context,
               const QueueFamilyIndices& family_indices) {
  const VkDevice& device = *context.device();
  SetQueue(device, family_indices.graphics, &graphics_queue_);
  SetQueue(device, family_indices.compute, &compute_queue_);
  SetQueue(device, family_indices.transfer, &transfer_queue_);
  if (family_indices.present.has_value()) {
    present_queue_.emplace();
    SetQueue(device, family_indices.present.value(), &present_queue_.value());
  }
}

void Queues::SetQueue(const VkDevice& device, uint32_t family_index,
                      Queue* queue) const {
  constexpr int kQueueIndex = 0;
  queue->family_index = family_index;
  vkGetDeviceQueue(device, family_index, kQueueIndex, &queue->queue);
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
