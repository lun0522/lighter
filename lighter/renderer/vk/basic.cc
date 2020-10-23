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
#include "lighter/renderer/vk/util.h"

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

const std::vector<const char*>& DebugCallback::GetRequiredLayers() {
  static const std::vector<const char*>* validation_layers = nullptr;
  if (validation_layers == nullptr) {
    validation_layers = new std::vector<const char*>{
        "VK_LAYER_KHRONOS_validation",
    };
  }
  return *validation_layers;
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
