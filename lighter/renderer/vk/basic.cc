//
//  basic.h
//
//  Created by Pujun Lun on 10/22/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/basic.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "lighter/common/util.h"
#include "lighter/renderer/ir/pipeline_util.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/property_checker.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/strings/str_format.h"

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
                            absl::Span<const std::string> required_properties) {
  LOG_INFO << absl::StreamFormat("Checking %s support", property_type);
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
bool SupportsSwapchain(intl::PhysicalDevice physical_device) {
  LOG_INFO << "Checking swapchain device extensions support";
  const auto checker = PropertyChecker::ForDeviceExtensions(physical_device);
  return checker.AreSupported({std::string{kSwapchainExtension}});
}

// Returns whether 'surfaces' are supported by 'physical_device'.
bool SupportsSurfaces(intl::PhysicalDevice physical_device,
                      absl::Span<const Surface* const> surfaces) {
  LOG_INFO << "Checking surfaces compatibility";

  uint32_t format_count, mode_count;
  for (int i = 0; i < surfaces.size(); ++i) {
    ASSERT_SUCCESS(
        physical_device.getSurfaceFormatsKHR(**surfaces[i], &format_count,
                                             /*pSurfaceFormats=*/nullptr),
        absl::StrFormat("Failed to query surface formats for window %d", i));
    ASSERT_SUCCESS(
        physical_device.getSurfacePresentModesKHR(**surfaces[i], &mode_count,
                                                  /*pPresentModes=*/nullptr),
        absl::StrFormat("Failed to query present modes for window %d", i));
    if (format_count == 0 || mode_count == 0) {
      LOG_INFO << absl::StrFormat("Not compatible with window %d", i);
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
    absl::Span<const intl::QueueFamilyProperties> properties_span) {
  return common::util::FindIndexOfFirstIf<intl::QueueFamilyProperties>(
      properties_span, FamilyHasQueue<flag>);
}

// Finds family indices of queues we need. If any queue is not found in the
// given 'physical_device', returns std::nullopt.
std::optional<PhysicalDevice::QueueFamilyIndices> FindDeviceQueues(
    intl::PhysicalDevice physical_device,
    absl::Span<const Surface* const> surfaces) {
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

  // Request support for anisotropy filtering.
  const auto features = physical_device.getFeatures();
  if (!features.samplerAnisotropy) {
    LOG_INFO << "Anisotropy filtering not supported";
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
    const auto has_present_support =
        [physical_device, surface, family_index](const auto&) mutable {
          return physical_device.getSurfaceSupportKHR(family_index++, surface);
        };
    const auto iter = std::find_if(properties_vector.begin(),
                                   properties_vector.end(),
                                   has_present_support);
    if (iter == properties_vector.end()) {
      LOG_INFO << absl::StreamFormat("No presentation queue for surface %d", i);
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
    absl::Span<const intl::SampleCountFlagBits> candidates) {
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

    case MultisamplingMode::kDecent:
      return GetFirstSupportedSampleCount(
          supported_sample_counts,
          {SampleCount::e4, SampleCount::e2, SampleCount::e1});

    case MultisamplingMode::kBest:
      return GetFirstSupportedSampleCount(
          supported_sample_counts,
          {SampleCount::e64, SampleCount::e32, SampleCount::e16,
           SampleCount::e8, SampleCount::e4, SampleCount::e2, SampleCount::e1});
  }
}

// Returns a callback that prints the debug message.
VKAPI_ATTR VkBool32 VKAPI_CALL UserCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  const auto severity =
      static_cast<intl::DebugUtilsMessageSeverityFlagBitsEXT>(message_severity);
  const auto types =
      static_cast<intl::DebugUtilsMessageTypeFlagsEXT>(message_types);
  const bool is_error =
      severity == intl::DebugUtilsMessageSeverityFlagBitsEXT::eError;

  LOG_SWITCH(is_error) << absl::StreamFormat(
      "[DebugCallback] severity %s, types %s, message:",
      intl::to_string(severity), intl::to_string(types));
  LOG_SWITCH(is_error) << callback_data->pMessage;
  return VK_FALSE;
}

}  // namespace

Instance::Instance(const Context* context, bool enable_validation,
                   const char* application_name,
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
  const auto application_info = intl::ApplicationInfo{}
      .setPApplicationName(application_name)
      .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
      .setPEngineName("Lighter")
      .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
      .setApiVersion(VK_API_VERSION_1_2);
  const auto instance_create_info = intl::InstanceCreateInfo{}
      .setPApplicationInfo(&application_info)
      .setPEnabledLayerNames(required_layers)
      .setPEnabledExtensionNames(required_extensions);
  instance_ = intl::createInstance(instance_create_info,
                                   *context_.host_allocator());
}

Instance::~Instance() {
  instance_.destroy(*context_.host_allocator());
}

DebugMessenger::DebugMessenger(const Context* context,
                               const ir::debug_message::Config& config)
    : context_{*FATAL_IF_NULL(context)} {
  // We may pass data to 'pUserData' which can be retrieved from the callback.
  const auto messenger_create_info = intl::DebugUtilsMessengerCreateInfoEXT{}
      .setMessageSeverity(
          type::ConvertDebugMessageSeverities(config.message_severities))
      .setMessageType(type::ConvertDebugMessageTypes(config.message_types))
      .setPfnUserCallback(UserCallback);
  messenger_ = context_.instance()->createDebugUtilsMessengerEXT(
      messenger_create_info, *context_.host_allocator());
}

DebugMessenger::~DebugMessenger() {
  context_.InstanceDestroy(messenger_);
}

Surface::Surface(const Context* context, const common::Window& window)
    : context_{*FATAL_IF_NULL(context)} {
  const auto create_surface_func = window.GetCreateSurfaceFunc();
  VkSurfaceKHR surface;
  ASSERT_SUCCESS(
    intl::Result{create_surface_func(
        *context_.instance(), context_.host_allocator().c_type(), &surface)},
    "Failed to create window surface");
  surface_ = surface;
}

Surface::~Surface() {
  context_.InstanceDestroy(surface_);
}

PhysicalDevice::PhysicalDevice(const Context* context,
                               absl::Span<Surface* const> surfaces)
    : context_{*FATAL_IF_NULL(context)} {
  LOG_INFO << "Selecting physical device";

  static const auto is_discrete_gpu = [](const auto& properties) {
    return properties.deviceType == intl::PhysicalDeviceType::eDiscreteGpu;
  };
  std::optional<intl::PhysicalDeviceProperties> properties;
  for (intl::PhysicalDevice candidate :
       context_.instance()->enumeratePhysicalDevices()) {
    const auto indices = FindDeviceQueues(candidate, surfaces);
    if (!indices.has_value()) {
      LOG_INFO << "Found unsupported features, keep searching";
      continue;
    }

    physical_device_ = candidate;
    queue_family_indices_ = indices.value();
    properties = candidate.getProperties();
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
  for (auto mode : {MultisamplingMode::kNone,
                    MultisamplingMode::kDecent,
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
  static const std::vector<float> queue_priorities{1.0f};
  std::vector<intl::DeviceQueueCreateInfo> queue_create_infos;
  for (uint32_t queue_family_index : queue_family_indices_set) {
    queue_create_infos.push_back(intl::DeviceQueueCreateInfo{}
        .setQueueFamilyIndex(queue_family_index)
        .setQueuePriorities(queue_priorities)
    );
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
  const auto required_features = intl::PhysicalDeviceFeatures{}
      .setSamplerAnisotropy(true);
  const auto device_create_info = intl::DeviceCreateInfo{}
      .setQueueCreateInfos(queue_create_infos)
      .setPEnabledLayerNames(required_layers)
      .setPEnabledExtensionNames(required_extensions)
      .setPEnabledFeatures(&required_features);
  device_ = physical_device->createDevice(device_create_info,
                                          *context_.host_allocator());
}

Device::~Device() {
  device_.destroy(*context_.host_allocator());
}

Queues::Queues(const Context& context) {
  // We simply use the first queue in the family with 'family_index'.
  constexpr int kQueueIndex = 0;
  const auto get_queue = [&context](uint32_t family_index) {
    return (*context.device()).getQueue(family_index, kQueueIndex);
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
