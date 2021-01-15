//
//  basic.h
//
//  Created by Pujun Lun on 10/22/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/basic.h"

#include <algorithm>
#include <optional>
#include <string>

#include "lighter/common/util.h"
#include "lighter/renderer/pipeline_util.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/debug_callback.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "third_party/absl/functional/function_ref.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter::renderer::vk {
namespace {

// Returns instance extensions required by 'windows'.
std::vector<const char*> GetWindowExtensions(
    absl::Span<const common::Window* const> windows) {
  absl::flat_hash_set<const char*> window_extensions;
  for (const common::Window* window : windows) {
    const auto required_extensions = window->GetRequiredExtensions();
    window_extensions.insert(required_extensions.begin(),
                             required_extensions.end());
  }
  return {window_extensions.begin(), window_extensions.end()};
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

// Returns whether swapchain is supported by 'physical_device'.
bool HasSwapchainSupport(const VkPhysicalDevice& physical_device,
                         absl::Span<const Surface* const> surfaces,
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
  const std::vector<std::string> required{swapchain_extensions.begin(),
                                          swapchain_extensions.end()};
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
// given 'physical_device', returns std::nullopt.
std::optional<PhysicalDevice::QueueFamilyIndices> FindDeviceQueues(
    const VkPhysicalDevice& physical_device,
    absl::Span<const Surface* const> surfaces,
    absl::Span<const char* const> swapchain_extensions) {
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  LOG_INFO << "Found device: " << properties.deviceName;
  LOG_EMPTY_LINE;

  // Request swapchain support if use window.
  if (!surfaces.empty() &&
      !HasSwapchainSupport(physical_device, surfaces, swapchain_extensions)) {
    return std::nullopt;
  }

  // Request support for anisotropy filtering.
  VkPhysicalDeviceFeatures feature_support;
  vkGetPhysicalDeviceFeatures(physical_device, &feature_support);
  if (!feature_support.samplerAnisotropy) {
    return std::nullopt;
  }

  // Find queue family that holds graphics queue and compute queue.
  PhysicalDevice::QueueFamilyIndices candidate{};
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
    return std::nullopt;
  } else {
    candidate.graphics = CAST_TO_UINT(graphics_queue_index.value());
  }

  static const auto has_compute_support =
      [](const VkQueueFamilyProperties& family) {
        return family.queueCount && (family.queueFlags & VK_QUEUE_COMPUTE_BIT);
      };
  const auto compute_queue_index =
      common::util::FindIndexOfFirst<VkQueueFamilyProperties>(
          families, has_compute_support);
  if (!compute_queue_index.has_value()) {
    return std::nullopt;
  } else {
    candidate.compute = CAST_TO_UINT(compute_queue_index.value());
  }

  // Find queue family that holds presentation queue if needed.
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
      return std::nullopt;
    } else {
      candidate.presents.push_back(CAST_TO_UINT(present_queue_index.value()));
    }
  }

  return candidate;
}

// Returns the first supported sample count among 'candidates'.
VkSampleCountFlagBits GetFirstSupportedSampleCount(
    VkSampleCountFlags supported,
    absl::Span<const VkSampleCountFlagBits> candidates) {
  for (auto candidate : candidates) {
    if (supported & candidate) {
      return candidate;
    }
  }
  FATAL("Failed to find sample count");
}

// Returns the sample count to use for 'multisampling_mode'.
VkSampleCountFlagBits ChooseSampleCount(
    MultisamplingMode multisampling_mode,
    VkSampleCountFlags supported_sample_counts) {
  switch (multisampling_mode) {
    case MultisamplingMode::kNone:
      return VK_SAMPLE_COUNT_1_BIT;

    case MultisamplingMode::kDecent:
      return GetFirstSupportedSampleCount(
          supported_sample_counts,
          {VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT,
           VK_SAMPLE_COUNT_1_BIT});

    case MultisamplingMode::kBest:
      return GetFirstSupportedSampleCount(
          supported_sample_counts,
          {VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT,
           VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_4_BIT,
           VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_1_BIT});
  }
}

}  // namespace

Instance::Instance(const Context* context, std::string_view application_name,
                   absl::Span<const common::Window* const> windows)
    : context_{*FATAL_IF_NULL(context)} {
  // Check required instance layers.
  std::vector<const char*> required_layers;
  // Request support for debug reports if needed.
  if (context_.is_validation_enabled()) {
    required_layers.insert(required_layers.end(),
                           DebugCallback::GetRequiredLayers().begin(),
                           DebugCallback::GetRequiredLayers().end());
  }
  const std::vector<std::string> layer_names{required_layers.begin(),
                                             required_layers.end()};
  CheckInstanceLayerSupport(layer_names);

  // Check required instance extensions.
  std::vector<const char*> required_extensions = GetWindowExtensions(windows);
  // Request support for pushing descriptors.
  required_extensions.push_back(
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  // Request support for debug reports if needed.
  if (context_.is_validation_enabled()) {
    required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  const std::vector<std::string> extension_names{required_extensions.begin(),
                                                 required_extensions.end()};
  CheckInstanceExtensionSupport(extension_names);

  // Might be useful for the driver to optimize for some graphics engine.
  const std::string application_name_string{application_name};
  const VkApplicationInfo app_info{
      VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = application_name_string.c_str(),
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "Lighter",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      VK_API_VERSION_1_2,
  };

  // Specify which global extensions and validation layers to use.
  const VkInstanceCreateInfo instance_info{
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      &app_info,
      CONTAINER_SIZE(required_layers),
      required_layers.data(),
      CONTAINER_SIZE(required_extensions),
      required_extensions.data(),
  };

  ASSERT_SUCCESS(
      vkCreateInstance(&instance_info, *context_.host_allocator(), &instance_),
      "Failed to create instance");
}

Instance::~Instance() {
  vkDestroyInstance(instance_, *context_.host_allocator());
}

Surface::Surface(const Context* context, const common::Window& window)
    : context_{*FATAL_IF_NULL(context)} {
  surface_ = window.CreateSurface(*context_.instance(),
                                  *context_.host_allocator());
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      *context_.physical_device(), surface_, &capabilities_);
}

Surface::~Surface() {
  vkDestroySurfaceKHR(*context_.instance(), surface_,
                      *context_.host_allocator());
}

PhysicalDevice::PhysicalDevice(
    const Context* context, absl::Span<const Surface* const> surfaces,
    absl::Span<const char* const> swapchain_extensions)
    : context_{*FATAL_IF_NULL(context)} {
  const auto physical_devices = util::QueryAttribute<VkPhysicalDevice>(
      [this](uint32_t* count, VkPhysicalDevice* physical_device) {
        return vkEnumeratePhysicalDevices(
            *context_.instance(), count, physical_device);
      }
  );

  for (const auto& candidate : physical_devices) {
    const auto indices = FindDeviceQueues(candidate, surfaces,
                                          swapchain_extensions);
    if (indices.has_value()) {
      physical_device_ = candidate;
      queue_family_indices_ = indices.value();

      // Query physical device limits.
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(physical_device_, &properties);
      limits_ = properties.limits;

      // Determine the sample count to use in each multisampling mode.
      const VkSampleCountFlags supported_sample_counts = std::min({
          limits_.framebufferColorSampleCounts,
          limits_.framebufferDepthSampleCounts,
          limits_.framebufferStencilSampleCounts,
      });
      for (const auto mode : {MultisamplingMode::kNone,
                              MultisamplingMode::kDecent,
                              MultisamplingMode::kBest}) {
        sample_count_map_.insert(
            {mode, ChooseSampleCount(mode, supported_sample_counts)});
      }

      return;
    }
  }
  FATAL("Failed to find suitable graphics device");
}

Device::Device(const Context* context,
               absl::Span<const char* const> swapchain_extensions)
    : context_{*FATAL_IF_NULL(context)} {
  // Specify which queues do we want to use.
  const auto& queue_family_indices =
      context_.physical_device().queue_family_indices();
  const absl::flat_hash_set<uint32_t> queue_family_indices_set{
      queue_family_indices.graphics,
      queue_family_indices.compute,
  };

  // Priority is always required even if there is only one queue.
  constexpr float kPriority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> queue_infos;
  for (uint32_t family_index : queue_family_indices_set) {
    queue_infos.push_back({
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = nullflag,
        family_index,
        .queueCount = 1,
        &kPriority,
    });
  }

  std::vector<const char*> enabled_layers;
  // Enable support for debug reports if needed.
  if (context_.is_validation_enabled()) {
    enabled_layers.insert(enabled_layers.end(),
                          DebugCallback::GetRequiredLayers().begin(),
                          DebugCallback::GetRequiredLayers().end());
  }

  std::vector<const char*> enabled_extensions;
  enabled_extensions.reserve(swapchain_extensions.size() + 2);
  // Enable support for negative-height viewport and pushing descriptors.
  enabled_extensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
  enabled_extensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
  // Enable support for swapchain if needed.
  enabled_extensions.insert(enabled_extensions.end(),
                            swapchain_extensions.begin(),
                            swapchain_extensions.end());

  // Enable support for anisotropy filtering.
  VkPhysicalDeviceFeatures required_features{};
  required_features.samplerAnisotropy = VK_TRUE;

  const VkDeviceCreateInfo device_info{
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      CONTAINER_SIZE(queue_infos),
      queue_infos.data(),
      CONTAINER_SIZE(enabled_layers),
      enabled_layers.data(),
      CONTAINER_SIZE(enabled_extensions),
      enabled_extensions.data(),
      &required_features,
  };

  ASSERT_SUCCESS(vkCreateDevice(*context_.physical_device(), &device_info,
                                *context_.host_allocator(), &device_),
                 "Failed to create logical device");
}

Device::~Device() {
  vkDestroyDevice(device_, *context_.host_allocator());
}

Queues::Queues(const Context& context) {
  // We simply use the first queue in the family with 'family_index'.
  constexpr int kQueueIndex = 0;
  const auto get_queue = [&context](uint32_t family_index) {
    VkQueue queue;
    vkGetDeviceQueue(*context.device(), family_index, kQueueIndex, &queue);
    return queue;
  };

  const auto& family_indices = context.physical_device().queue_family_indices();
  graphics_queue_ = get_queue(family_indices.graphics);
  graphics_queue_ = get_queue(family_indices.compute);
  present_queues_.reserve(family_indices.presents.size());
  for (uint32_t family_index : family_indices.presents) {
    present_queues_.push_back(get_queue(family_index));
  }
}

}  // namespace vk::renderer::lighter
