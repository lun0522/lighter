//
//  basic.h
//
//  Created by Pujun Lun on 10/22/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/basic.h"

#include <algorithm>
#include <string>

#include "lighter/common/util.h"
#include "lighter/renderer/pipeline_util.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/property_checker.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter::renderer::vk {
namespace {

static constexpr char* kSwapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
static constexpr char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
static constexpr char* kValidationExtension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

// Uses 'property_checker' to check if all 'required_properties' are supported,
// and throws a runtime exception if any unsupported exists.
void CheckPropertiesSupport(std::string_view property_type,
                            const PropertyChecker& property_checker,
                            absl::Span<const std::string> required_properties) {
  LOG_INFO << absl::StreamFormat("Checking %s support", property_type);
  LOG_EMPTY_LINE;
  ASSERT_EMPTY(property_checker.FindUnsupported(required_properties),
               absl::StrFormat("Found unsupported %s", property_type));
}

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

// Returns whether swapchain is supported by 'physical_device'.
bool SupportsSwapchain(const VkPhysicalDevice& physical_device) {
  LOG_INFO << "Checking swapchain device extensions support";
  LOG_EMPTY_LINE;

  const auto checker = PropertyChecker::ForDeviceExtensions(physical_device);
  return checker.AreSupported({kSwapchainExtension});
}

// Returns whether 'surfaces' are supported by 'physical_device'.
bool SupportsSurfaces(const VkPhysicalDevice& physical_device,
                      absl::Span<const Surface* const> surfaces) {
  LOG_INFO << "Checking surfaces compatibility";
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

  LOG_INFO << "All compatible";
  LOG_EMPTY_LINE;
  return true;
}

// Finds family indices of queues we need. If any queue is not found in the
// given 'physical_device', returns std::nullopt.
std::optional<PhysicalDevice::QueueFamilyIndices> FindDeviceQueues(
    const VkPhysicalDevice& physical_device,
    absl::Span<const Surface* const> surfaces) {
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  LOG_INFO << "Found graphics device: " << properties.deviceName;
  LOG_EMPTY_LINE;

  // Request swapchain and surface support if use window.
  if (!surfaces.empty()) {
    if (!SupportsSwapchain(physical_device) ||
        !SupportsSurfaces(physical_device, surfaces)) {
      return std::nullopt;
    }
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
      common::util::FindIndexOfFirstIf<VkQueueFamilyProperties>(
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
      common::util::FindIndexOfFirstIf<VkQueueFamilyProperties>(
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
        common::util::FindIndexOfFirstIf<VkQueueFamilyProperties>(
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

Instance::Instance(const Context* context, bool enable_validation,
                   std::string_view application_name,
                   absl::Span<const common::Window* const> windows)
    : context_{*FATAL_IF_NULL(context)} {
  // Check required instance layers.
  std::vector<const char*> required_layers;
  // Request support for validation if needed.
  if (enable_validation) {
    required_layers.push_back(kValidationLayer);
  }
  CheckPropertiesSupport(
      "instance layers", PropertyChecker::ForInstanceLayers(),
      std::vector<std::string>{required_layers.begin(), required_layers.end()});

  // Check required instance extensions.
  std::vector<const char*> required_extensions = GetWindowExtensions(windows);
  // Request support for pushing descriptors.
  required_extensions.push_back(
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  // Request support for validation if needed.
  if (enable_validation) {
    required_extensions.push_back(kValidationExtension);
  }
  CheckPropertiesSupport(
      "instance extensions", PropertyChecker::ForInstanceExtensions(),
      std::vector<std::string>{required_extensions.begin(),
                               required_extensions.end()});

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
}

Surface::~Surface() {
  vkDestroySurfaceKHR(*context_.instance(), surface_,
                      *context_.host_allocator());
}

PhysicalDevice::PhysicalDevice(const Context* context,
                               absl::Span<Surface* const> surfaces)
    : context_{*FATAL_IF_NULL(context)} {
  const auto physical_devices = util::QueryAttribute<VkPhysicalDevice>(
      [this](uint32_t* count, VkPhysicalDevice* physical_device) {
        return vkEnumeratePhysicalDevices(
            *context_.instance(), count, physical_device);
      }
  );

  for (const auto& candidate : physical_devices) {
    const auto indices = FindDeviceQueues(candidate, surfaces);
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

      // Query surface capabilities.
      for (Surface* surface : surfaces) {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physical_device_, **surface, &capabilities);
        surface->SetCapabilities(std::move(capabilities));
      }

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

Device::Device(const Context* context, bool enable_validation,
               bool enable_swapchain)
    : context_{*FATAL_IF_NULL(context)} {
  // Specify which queues do we want to use.
  const PhysicalDevice& physical_device = context_.physical_device();
  const auto& queue_family_indices = physical_device.queue_family_indices();
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

  std::vector<const char*> required_layers;
  // Request support for validation if needed.
  if (enable_validation) {
    required_layers.push_back(kValidationLayer);
  }
  CheckPropertiesSupport(
      "device layers", PropertyChecker::ForDeviceLayers(*physical_device),
      std::vector<std::string>{required_layers.begin(), required_layers.end()});

  std::vector<const char*> required_extensions{
      // Request support for negative-height viewport.
      VK_KHR_MAINTENANCE1_EXTENSION_NAME,
      // Request support for pushing descriptors.
      VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
  };
  // Request support for swapchain if needed.
  if (enable_swapchain) {
    required_extensions.push_back(kSwapchainExtension);
  }
  const auto device_extensions_checker =
      PropertyChecker::ForDeviceExtensions(*physical_device);
  CheckPropertiesSupport(
      "device extensions", device_extensions_checker,
      std::vector<std::string>{required_extensions.begin(),
                               required_extensions.end()});

  // According to the spec, VK_KHR_portability_subset must be included if it is
  // included in vkEnumerateDeviceExtensionProperties:
  // https://vulkan.lunarg.com/doc/view/1.2.176.1/mac/1.2-extensions/vkspec.html#VUID-VkDeviceCreateInfo-pProperties-04451
  // TODO: Remove #define once this extension gets out of beta.
  #ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
  #define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
  for (const char* extension : {VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME}) {
    if (device_extensions_checker.IsSupported(extension)) {
      required_extensions.push_back(extension);
      LOG_INFO << absl::StreamFormat("Also including %s as required by spec",
                                     extension);
    }
  }
  #endif  // VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME

  // Request support for anisotropy filtering. This should be safe as we have
  // checked that during physical device creation.
  VkPhysicalDeviceFeatures required_features{};
  required_features.samplerAnisotropy = VK_TRUE;

  const VkDeviceCreateInfo device_info{
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      CONTAINER_SIZE(queue_infos),
      queue_infos.data(),
      CONTAINER_SIZE(required_layers),
      required_layers.data(),
      CONTAINER_SIZE(required_extensions),
      required_extensions.data(),
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

}  // namespace lighter::renderer::vk
