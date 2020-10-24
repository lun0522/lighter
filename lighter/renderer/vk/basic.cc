//
//  basic.h
//
//  Created by Pujun Lun on 10/22/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/basic.h"

#include <string>

#include "lighter/common/util.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/debug_callback.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/optional.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace {

// Checks support for 'required' extensions, and throws a runtime exception
// if any of them is not supported.
void CheckInstanceExtensionSupport(absl::Span<const std::string> required) {
  LOG_INFO << "Checking instance extension support...";
  LOG_EMPTY_LINE;

  static const auto properties = util::QueryAttribute<VkExtensionProperties>(
      [](uint32_t* count, VkExtensionProperties* properties) {
        return vkEnumerateInstanceExtensionProperties(
            /*pLayerName=*/nullptr, count, properties);
      }
  );
  static const auto get_name = [](const VkExtensionProperties& property) {
    return property.extensionName;
  };
  const auto unsupported = util::FindUnsupported<VkExtensionProperties>(
      required, properties, get_name);
  ASSERT_NO_VALUE(unsupported,
                  absl::StrCat("Unsupported extension: ", unsupported.value()));
}

// Checks support for 'required' layers, and throws a runtime exception if any
// of them is not supported.
void CheckInstanceLayerSupport(absl::Span<const std::string> required) {
  LOG_INFO << "Checking instance layer support...";
  LOG_EMPTY_LINE;

  static const auto properties = util::QueryAttribute<VkLayerProperties>(
      [](uint32_t* count, VkLayerProperties* properties) {
        return vkEnumerateInstanceLayerProperties(count, properties);
      }
  );
  static const auto get_name = [](const VkLayerProperties& property) {
    return property.layerName;
  };
  const auto unsupported = util::FindUnsupported<VkLayerProperties>(
      required, properties, get_name);
  ASSERT_NO_VALUE(unsupported,
                  absl::StrCat("Unsupported layer: ", unsupported.value()));
}

// Returns whether swapchain is supported.
bool HasSwapchainSupport(const VkPhysicalDevice& physical_device,
                         absl::Span<const Surface*> surfaces,
                         absl::Span<const char* const> swapchain_extensions) {
  LOG_INFO << "Checking window support...";
  LOG_EMPTY_LINE;

  uint32_t format_count, mode_count;
  for (int i = 0; i < surfaces.size(); ++i) {
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, **surfaces[i], &format_count,
        /*pSurfaceFormats=*/nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, **surfaces[i], &mode_count, /*pPresentModes=*/nullptr);

    if (format_count == 0 || mode_count == 0) {
      LOG_INFO << absl::StrFormat("Not compatible with window %d", i);
      return false;
    }
  }

  LOG_INFO << "Checking swapchain support...";
  LOG_EMPTY_LINE;

  // Query support for device extensions.
  const std::vector<std::string> required{
      swapchain_extensions.begin(), swapchain_extensions.end()};
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

  return true;
}

// Finds family indices of queues we need. If any queue is not found in the
// given 'physical_device', returns absl::nullopt.
absl::optional<QueueFamilyIndices> FindDeviceQueues(
    const VkPhysicalDevice& physical_device,
    absl::Span<const Surface*> surfaces,
    absl::Span<const char* const> swapchain_extensions) {
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  LOG_INFO << "Found device: " << properties.deviceName;
  LOG_EMPTY_LINE;

  // Request swapchain support if use window.
  if (!surfaces.empty() &&
      !HasSwapchainSupport(physical_device, surfaces, swapchain_extensions)) {
    return absl::nullopt;
  }

  // Request support for anisotropy filtering.
  VkPhysicalDeviceFeatures feature_support;
  vkGetPhysicalDeviceFeatures(physical_device, &feature_support);
  if (!feature_support.samplerAnisotropy) {
    return absl::nullopt;
  }

  // Find queue family that holds graphics queue.
  QueueFamilyIndices candidate{};
  const auto families = util::QueryAttribute<VkQueueFamilyProperties>(
      [&physical_device](uint32_t* count, VkQueueFamilyProperties* properties) {
        return vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, count, properties);
      }
  );

  static const auto has_graphics_support =
      [](const VkQueueFamilyProperties& family) {
        return family.queueCount && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
      };
  const auto graphics_queue_index =
      common::util::FindIndexOfFirst<VkQueueFamilyProperties>(
          families, has_graphics_support);
  if (!graphics_queue_index.has_value()) {
    return absl::nullopt;
  } else {
    candidate.graphics = static_cast<uint32_t>(graphics_queue_index.value());
  }

  static const auto has_compute_support =
      [](const VkQueueFamilyProperties& family) {
        return family.queueCount && (family.queueFlags & VK_QUEUE_COMPUTE_BIT);
      };
  const auto compute_queue_index =
      common::util::FindIndexOfFirst<VkQueueFamilyProperties>(
          families, has_compute_support);
  if (!compute_queue_index.has_value()) {
    return absl::nullopt;
  } else {
    candidate.compute = static_cast<uint32_t>(compute_queue_index.value());
  }

  // Find queue family that holds presentation queue if use window.
  candidate.presents.reserve(surfaces.size());
  for (const auto* surface : surfaces) {
    uint32_t index = 0;
    const auto has_present_support = [&physical_device, surface, index]
        (const VkQueueFamilyProperties& family) mutable {
      VkBool32 support = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index++,
                                           **surface, &support);
      return support;
    };
    const auto present_queue_index =
        common::util::FindIndexOfFirst<VkQueueFamilyProperties>(
            families, has_present_support);
    if (!present_queue_index.has_value()) {
      return absl::nullopt;
    } else {
      candidate.presents.push_back(
          static_cast<uint32_t>(present_queue_index.value()));
    }
  }

  return candidate;
}

} /* namespace */

absl::flat_hash_set<uint32_t>
QueueFamilyIndices::GetUniqueFamilyIndices() const {
  absl::flat_hash_set<uint32_t> indices_set{presents.begin(), presents.end()};
  indices_set.insert(graphics);
  indices_set.insert(compute);
  return indices_set;
}

Instance::Instance(const Context* context,
                   absl::Span<const char* const> window_extensions)
    : context_{*FATAL_IF_NULL(context)} {
  std::vector<const char*> instance_extensions{
      window_extensions.begin(), window_extensions.end()};
  // Request support for pushing descriptors.
  instance_extensions.push_back(
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  // Request support for debug reports.
  if (context_.is_validation_enabled()) {
    instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  const std::vector<std::string> extension_names{
      instance_extensions.begin(), instance_extensions.end()};
  CheckInstanceExtensionSupport(extension_names);

  std::vector<const char*> instance_layers;
  // Request support for debug reports.
  if (context_.is_validation_enabled()) {
    instance_layers.insert(instance_layers.end(),
                           DebugCallback::GetRequiredLayers().begin(),
                           DebugCallback::GetRequiredLayers().end());
  }
  const std::vector<std::string> layer_names{instance_layers.begin(),
                                             instance_layers.end()};
  CheckInstanceLayerSupport(layer_names);

  // [optional]
  // Might be useful for the driver to optimize for some graphics engine.
  const VkApplicationInfo app_info{
      VK_STRUCTURE_TYPE_APPLICATION_INFO,
      /*pNext=*/nullptr,
      /*pApplicationName=*/context_.application_name().c_str(),
      /*applicationVersion=*/VK_MAKE_VERSION(1, 0, 0),
      /*pEngineName=*/"Lighter",
      /*engineVersion=*/VK_MAKE_VERSION(1, 0, 0),
      VK_API_VERSION_1_2,
  };

  // [required]
  // Specify which global extensions and validation layers to use.
  const VkInstanceCreateInfo instance_info{
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      &app_info,
      CONTAINER_SIZE(instance_layers),
      instance_layers.data(),
      CONTAINER_SIZE(instance_extensions),
      instance_extensions.data(),
  };

  ASSERT_SUCCESS(
      vkCreateInstance(&instance_info, *context_.host_allocator(), &instance_),
      "Failed to create instance");
}

Instance::~Instance() {
  vkDestroyInstance(instance_, *context_.host_allocator());
}

Surface::Surface(const Context* context, const common::Window& window)
    : context_{*FATAL_IF_NULL(context)},
      surface_{window.CreateSurface(*context_.instance(),
                                    *context_.host_allocator())} {}

VkSurfaceCapabilitiesKHR Surface::GetCapabilities() const {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      *context_.physical_device(), surface_, &capabilities);
  return capabilities;
}

Surface::~Surface() {
  vkDestroySurfaceKHR(*context_.instance(), surface_,
                      *context_.host_allocator());
}

PhysicalDevice::PhysicalDevice(
    const Context* context, absl::Span<const Surface*> surfaces,
    absl::Span<const char* const> swapchain_extensions)
    : context_{*FATAL_IF_NULL(context)} {
  // Find all physical devices.
  const auto physical_devices = util::QueryAttribute<VkPhysicalDevice>(
      [this](uint32_t* count, VkPhysicalDevice* physical_device) {
        return vkEnumeratePhysicalDevices(
            *context_.instance(), count, physical_device);
      }
  );

  // Find a suitable device. If window support is requested, also request for
  // the swapchain and presentation queue here.
  for (const auto& candidate : physical_devices) {
    const auto indices = FindDeviceQueues(candidate, surfaces,
                                          swapchain_extensions);
    if (indices.has_value()) {
      physical_device_ = candidate;
      queue_family_indices_ = indices.value();

      // Query physical device limits.
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(physical_device_, &properties);
      physical_device_limits_ = properties.limits;

      return;
    }
  }
  FATAL("Failed to find suitable graphics device");
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
