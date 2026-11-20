//
//  basic.h
//
//  Created by Pujun Lun on 10/22/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/basic.h"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <string_view>
#include <unordered_set>

#include "lighter/common/util.h"
#include "lighter/renderer/ir/pipeline_util.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/property_checker.h"
#include "lighter/renderer/vk/type_mapping.h"

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif  // VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME

namespace lighter::renderer::vk {
namespace {

using ir::MultisamplingMode;

const char* kSwapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
const char* kValidationExtension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

// Uses 'property_checker' to check if all 'required_properties' are supported,
// and throws a runtime exception if any unsupported exists.
void CheckPropertiesSupport(std::string_view property_type,
                            const PropertyChecker& property_checker,
                            std::span<const std::string> required_properties) {
  LOG_INFO << std::format("Checking {} support", property_type);
  ASSERT_EMPTY(property_checker.FindUnsupported(required_properties),
               std::format("Found unsupported {}", property_type));
}

// Returns instance extensions required by 'windows'.
std::vector<const char*> GetWindowExtensions(
    std::span<const common::Window* const> windows) {
  std::unordered_set<const char*> window_extensions;
  for (const common::Window* window : windows) {
    const auto required_extensions = window->GetRequiredExtensions();
    window_extensions.insert(required_extensions.begin(),
                             required_extensions.end());
  }
  return {window_extensions.begin(), window_extensions.end()};
}

// Returns whether swapchain is supported by 'physical_device'.
bool SupportsSwapchain(intl::PhysicalDevice physical_device) {
  LOG_INFO << "Checking swapchain device extensions support";
  const auto checker = PropertyChecker::ForDeviceExtensions(physical_device);
  const std::array required{std::string{kSwapchainExtension}};
  return checker.AreSupported(required);
}

// Returns whether 'surfaces' are supported by 'physical_device'.
bool SupportsSurfaces(intl::PhysicalDevice physical_device,
                      std::span<const Surface* const> surfaces) {
  LOG_INFO << "Checking surfaces compatibility";

  uint32_t format_count, mode_count;
  for (int i = 0; i < surfaces.size(); ++i) {
    ASSERT_SUCCESS(
        physical_device.getSurfaceFormatsKHR(**surfaces[i], &format_count,
                                             /*pSurfaceFormats=*/nullptr),
        std::format("Failed to query surface formats for window {}", i));
    ASSERT_SUCCESS(
        physical_device.getSurfacePresentModesKHR(**surfaces[i], &mode_count,
                                                  /*pPresentModes=*/nullptr),
        std::format("Failed to query present modes for window {}", i));
    if (format_count == 0 || mode_count == 0) {
      LOG_INFO << std::format("Not compatible with window {}", i);
      return false;
    }
  }

  LOG_INFO << "All compatible";
  LOG_INFO;
  return true;
}

// Returns true if the queue family supports 'flag'.
template <intl::QueueFlagBits flag>
bool FamilyHasQueue(const intl::QueueFamilyProperties& properties) {
  return properties.queueCount && (properties.queueFlags & flag);
}

// Returns the index of the first queue family that supports 'flag'.
template <intl::QueueFlagBits flag>
std::optional<int> GetQueueFamilyIndex(
    std::span<const intl::QueueFamilyProperties> properties_span) {
  return common::util::FindIndexOfFirstIf<intl::QueueFamilyProperties>(
      properties_span, FamilyHasQueue<flag>);
}

// Finds family indices of queues we need. If any queue is not found in the
// given 'physical_device', returns std::nullopt.
std::optional<PhysicalDevice::QueueFamilyIndices> FindDeviceQueues(
    intl::PhysicalDevice physical_device,
    std::span<const Surface* const> surfaces) {
  const auto properties = physical_device.getProperties();
  LOG_INFO << "Found graphics device: " << properties.deviceName;
  LOG_INFO;

  // Request swapchain and surface support if use window.
  if (!surfaces.empty()) {
    if (!SupportsSwapchain(physical_device) ||
        !SupportsSurfaces(physical_device, surfaces)) {
      return std::nullopt;
    }
  }

  // Request required Vulkan 1.0, 1.2, and 1.3 device features.
  const auto feature_chain =
      physical_device.getFeatures2<intl::PhysicalDeviceFeatures2,
                                   intl::PhysicalDeviceVulkan12Features,
                                   intl::PhysicalDeviceVulkan13Features>();
  const auto& features =
      feature_chain.get<intl::PhysicalDeviceFeatures2>().features;
  if (!features.samplerAnisotropy) {
    LOG_INFO << "Anisotropy filtering not supported";
    return std::nullopt;
  }
  const auto& features12 =
      feature_chain.get<intl::PhysicalDeviceVulkan12Features>();
  if (!features12.timelineSemaphore) {
    LOG_INFO << "Vulkan 1.2 timelineSemaphore not supported";
    return std::nullopt;
  }
  if (!features12.descriptorIndexing ||
      !features12.shaderSampledImageArrayNonUniformIndexing ||
      !features12.descriptorBindingPartiallyBound ||
      !features12.descriptorBindingVariableDescriptorCount ||
      !features12.runtimeDescriptorArray) {
    LOG_INFO << "Vulkan 1.2 descriptor indexing features not supported";
    return std::nullopt;
  }
  const auto& features13 =
      feature_chain.get<intl::PhysicalDeviceVulkan13Features>();
  if (!features13.dynamicRendering) {
    LOG_INFO << "Vulkan 1.3 dynamicRendering not supported";
    return std::nullopt;
  }
  if (!features13.synchronization2) {
    LOG_INFO << "Vulkan 1.3 synchronization2 not supported";
    return std::nullopt;
  }

  // Find a queue family that holds graphics queue and compute queue.
  const auto properties_vector = physical_device.getQueueFamilyProperties();
  PhysicalDevice::QueueFamilyIndices candidate{};

  const std::optional<int> graphics_index =
      GetQueueFamilyIndex<intl::QueueFlagBits::eGraphics>(properties_vector);
  if (!graphics_index.has_value()) {
    LOG_INFO << "No graphics queue";
    return std::nullopt;
  }
  candidate.graphics = CAST_TO_UINT(graphics_index.value());

  const std::optional<int> compute_index =
      GetQueueFamilyIndex<intl::QueueFlagBits::eCompute>(properties_vector);
  if (!compute_index.has_value()) {
    LOG_INFO << "No compute queue";
    return std::nullopt;
  }
  candidate.compute = CAST_TO_UINT(compute_index.value());

  // Find a queue family that holds presentation queues if needed.
  candidate.presents.reserve(surfaces.size());
  for (int i = 0; i < surfaces.size(); ++i) {
    const intl::SurfaceKHR surface = **surfaces[i];
    uint32_t family_index = 0;
    const auto has_present_support = [physical_device, surface,
                                      family_index](const auto&) mutable {
      return physical_device.getSurfaceSupportKHR(family_index++, surface);
    };
    const auto iter =
        std::find_if(properties_vector.begin(), properties_vector.end(),
                     has_present_support);
    if (iter == properties_vector.end()) {
      LOG_INFO << std::format("No presentation queue for surface {}", i);
      return std::nullopt;
    }
    candidate.presents.push_back(
        CAST_TO_UINT(std::distance(properties_vector.begin(), iter)));
  }

  return candidate;
}

// Returns the first supported sample count among 'candidates'.
intl::SampleCountFlagBits GetFirstSupportedSampleCount(
    intl::SampleCountFlags supported,
    std::span<const intl::SampleCountFlagBits> candidates) {
  for (intl::SampleCountFlagBits candidate : candidates) {
    if (supported & candidate) {
      return candidate;
    }
  }
  FATAL("Failed to find sample count");
}

// Returns the sample count to use for 'multisampling_mode'.
intl::SampleCountFlagBits ChooseSampleCount(
    MultisamplingMode multisampling_mode,
    intl::SampleCountFlags supported_sample_counts) {
  using SampleCount = intl::SampleCountFlagBits;
  switch (multisampling_mode) {
    case MultisamplingMode::kNone:
      return SampleCount::e1;

    case MultisamplingMode::kDecent: {
      constexpr std::array candidates{SampleCount::e4, SampleCount::e2,
                                      SampleCount::e1};
      return GetFirstSupportedSampleCount(supported_sample_counts, candidates);
    }

    case MultisamplingMode::kBest: {
      constexpr std::array candidates{
          SampleCount::e64, SampleCount::e32, SampleCount::e16, SampleCount::e8,
          SampleCount::e4,  SampleCount::e2,  SampleCount::e1};
      return GetFirstSupportedSampleCount(supported_sample_counts, candidates);
    }
  }
}

// Returns a callback that prints the debug message.
VKAPI_ATTR intl::Bool32 VKAPI_CALL
UserCallback(intl::DebugUtilsMessageSeverityFlagBitsEXT severity,
             intl::DebugUtilsMessageTypeFlagsEXT types,
             const intl::DebugUtilsMessengerCallbackDataEXT* callback_data,
             void* user_data) {
  const bool is_error =
      severity == intl::DebugUtilsMessageSeverityFlagBitsEXT::eError;

  LOG_SWITCH(is_error) << std::format(
      "[DebugCallback] severity {}, types {}, message:",
      intl::to_string(severity), intl::to_string(types));
  LOG_SWITCH(is_error) << callback_data->pMessage;
  return intl::False;
}

}  // namespace

Instance::Instance(const Context* context, bool enable_validation,
                   const char* application_name,
                   std::span<const common::Window* const> windows) {
  FATAL_IF_NULL(context);
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
  const auto instance_extensions_checker =
      PropertyChecker::ForInstanceExtensions();
  std::vector<const char*> required_extensions = GetWindowExtensions(windows);
  // Request support for validation if needed.
  if (enable_validation) {
    required_extensions.push_back(kValidationExtension);
  }
  intl::InstanceCreateFlags instance_flags{};
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
  if (instance_extensions_checker.IsSupported(
          VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    required_extensions.push_back(
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    instance_flags |= intl::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
  }
#endif  // VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
  CheckPropertiesSupport("instance extensions", instance_extensions_checker,
                         std::vector<std::string>{required_extensions.begin(),
                                                  required_extensions.end()});

  // Might be useful for the driver to optimize for some graphics engine.
  const auto application_info =
      intl::ApplicationInfo{}
          .setPApplicationName(application_name)
          .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
          .setPEngineName("Lighter")
          .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
          .setApiVersion(VK_API_VERSION_1_3);
  const auto instance_create_info =
      intl::InstanceCreateInfo{}
          .setFlags(instance_flags)
          .setPApplicationInfo(&application_info)
          .setPEnabledLayerNames(required_layers)
          .setPEnabledExtensionNames(required_extensions);
  instance_ = intl::raii::Instance{loader_context_, instance_create_info,
                                   *context->host_allocator()};
}

DebugMessenger::DebugMessenger(const Context* context,
                               const ir::debug_message::Config& config) {
  FATAL_IF_NULL(context);
  // We may pass data to 'pUserData' which can be retrieved from the callback.
  const auto messenger_create_info =
      intl::DebugUtilsMessengerCreateInfoEXT{}
          .setMessageSeverity(
              type::ConvertDebugMessageSeverities(config.message_severities))
          .setMessageType(type::ConvertDebugMessageTypes(config.message_types))
          .setPfnUserCallback(UserCallback);
  messenger_ = intl::raii::DebugUtilsMessengerEXT{context->instance().handle(),
                                                  messenger_create_info,
                                                  *context->host_allocator()};
}

Surface::Surface(const Context* context, const common::Window& window) {
  FATAL_IF_NULL(context);
  const auto create_surface_func = window.GetCreateSurfaceFunc();
  VkSurfaceKHR surface;
  ASSERT_SUCCESS(
      intl::Result{create_surface_func(
          *context->instance(), context->host_allocator().c_type(), &surface)},
      "Failed to create window surface");
  surface_ = intl::raii::SurfaceKHR{context->instance().handle(), surface,
                                    *context->host_allocator()};
}

PhysicalDevice::PhysicalDevice(const Context* context,
                               std::span<Surface* const> surfaces) {
  FATAL_IF_NULL(context);
  LOG_INFO << "Selecting physical device";

  static const auto is_discrete_gpu = [](const auto& properties) {
    return properties.deviceType == intl::PhysicalDeviceType::eDiscreteGpu;
  };
  std::optional<intl::PhysicalDeviceProperties> properties;
  for (intl::raii::PhysicalDevice& candidate :
       context->instance().handle().enumeratePhysicalDevices()) {
    const auto indices = FindDeviceQueues(*candidate, surfaces);
    if (!indices.has_value()) {
      LOG_INFO << "Found unsupported features, keep searching";
      continue;
    }

    physical_device_ = std::move(candidate);
    queue_family_indices_ = indices.value();
    properties = physical_device_.getProperties();
    limits_ = properties->limits;

    // Prefer discrete GPUs.
    if (is_discrete_gpu(properties.value())) {
      LOG_INFO << "Use discrete GPU: " << properties->deviceName;
      break;
    } else {
      LOG_INFO << "Not a discrete GPU, keep searching";
    }
  }

  ASSERT_HAS_VALUE(properties, "Failed to find suitable graphics device");
  if (!is_discrete_gpu(properties.value())) {
    LOG_INFO << "Use previously found GPU: " << properties->deviceName;
  }
  LOG_INFO;

  // Determine the sample count to use in each multisampling mode.
  const intl::SampleCountFlags supported_sample_counts = std::min({
      limits_.framebufferColorSampleCounts,
      limits_.framebufferDepthSampleCounts,
      limits_.framebufferStencilSampleCounts,
  });
  for (auto mode : {MultisamplingMode::kNone, MultisamplingMode::kDecent,
                    MultisamplingMode::kBest}) {
    sample_count_map_.insert(
        {mode, ChooseSampleCount(mode, supported_sample_counts)});
  }

  // Query surface capabilities.
  for (Surface* surface : surfaces) {
    surface->SetCapabilities(
        physical_device_.getSurfaceCapabilitiesKHR(**surface));
  }
}

Device::Device(const Context* context, bool enable_swapchain) {
  FATAL_IF_NULL(context);
  // Specify which queues do we want to use.
  const PhysicalDevice& physical_device = context->physical_device();
  const auto& queue_family_indices = physical_device.queue_family_indices();
  const std::unordered_set<uint32_t> queue_family_indices_set{
      queue_family_indices.graphics,
      queue_family_indices.compute,
  };

  // Priority is always required even if there is only one queue.
  static const std::vector<float> queue_priorities{1.0f};
  std::vector<intl::DeviceQueueCreateInfo> queue_create_infos;
  for (uint32_t queue_family_index : queue_family_indices_set) {
    queue_create_infos.push_back(intl::DeviceQueueCreateInfo{}
                                     .setQueueFamilyIndex(queue_family_index)
                                     .setQueuePriorities(queue_priorities));
  }

  std::vector<const char*> required_extensions{
      // Request support for pushing descriptors.
      VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
  };
  // Request support for swapchain if needed.
  if (enable_swapchain) {
    required_extensions.push_back(kSwapchainExtension);
  }
  const auto device_extensions_checker =
      PropertyChecker::ForDeviceExtensions(*physical_device);
  CheckPropertiesSupport("device extensions", device_extensions_checker,
                         std::vector<std::string>{required_extensions.begin(),
                                                  required_extensions.end()});

  // According to the spec, VK_KHR_portability_subset must be included if it is
  // supported by the physical device:
  // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkDeviceCreateInfo-pProperties-04451
  if (device_extensions_checker.IsSupported(
          VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
    required_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    LOG_INFO << std::format("Also including {} as required by spec",
                            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
  }

  // Request required Vulkan 1.0, 1.2, and 1.3 device features. These were
  // already verified during physical device selection.
  intl::StructureChain<intl::DeviceCreateInfo, intl::PhysicalDeviceFeatures2,
                       intl::PhysicalDeviceVulkan12Features,
                       intl::PhysicalDeviceVulkan13Features>
      device_create_chain{
          intl::DeviceCreateInfo{}
              .setQueueCreateInfos(queue_create_infos)
              .setPEnabledExtensionNames(required_extensions),
          intl::PhysicalDeviceFeatures2{}.setFeatures(
              intl::PhysicalDeviceFeatures{}.setSamplerAnisotropy(true)),
          intl::PhysicalDeviceVulkan12Features{}
              .setDescriptorIndexing(true)
              .setShaderSampledImageArrayNonUniformIndexing(true)
              .setDescriptorBindingPartiallyBound(true)
              .setDescriptorBindingVariableDescriptorCount(true)
              .setRuntimeDescriptorArray(true)
              .setTimelineSemaphore(true),
          intl::PhysicalDeviceVulkan13Features{}
              .setSynchronization2(true)
              .setDynamicRendering(true),
  };
  device_ =
      intl::raii::Device{physical_device.handle(),
                         device_create_chain.get<intl::DeviceCreateInfo>(),
                         *context->host_allocator()};
}

Queues::Queues(const Context& context) {
  // We simply use the first queue in the family with 'family_index'.
  constexpr int kQueueIndex = 0;
  const auto get_queue = [&context](uint32_t family_index) {
    return (*context.device()).getQueue(family_index, kQueueIndex);
  };

  const auto& family_indices = context.physical_device().queue_family_indices();
  graphics_queue_ = get_queue(family_indices.graphics);
  compute_queue_ = get_queue(family_indices.compute);
  present_queues_.reserve(family_indices.presents.size());
  for (uint32_t family_index : family_indices.presents) {
    present_queues_.push_back(get_queue(family_index));
  }
}

}  // namespace lighter::renderer::vk
