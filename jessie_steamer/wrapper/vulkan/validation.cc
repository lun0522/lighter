//
//  validation.cc
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/validation.h"

#include <iostream>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::string;
using std::vector;

VKAPI_ATTR VkBool32 VKAPI_CALL UserCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  // if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
  std::cout << "[Validation] " << callback_data->pMessage << std::endl;
  return VK_FALSE;
}

} /* namespace */

namespace validation {

const vector<const char*>& layers() {
  static vector<const char*>* kValidationLayers = nullptr;
  if (kValidationLayers == nullptr) {
    kValidationLayers = new vector<const char*>{
        "VK_LAYER_LUNARG_standard_validation",
    };
  }
  return *kValidationLayers;
}

void EnsureInstanceExtensionSupport(const vector<string>& required) {
  std::cout << "Checking instance extension support..."
            << std::endl << std::endl;

  auto properties {QueryAttribute<VkExtensionProperties>(
      [](uint32_t* count, VkExtensionProperties* properties) {
        return vkEnumerateInstanceExtensionProperties(
            nullptr, count, properties);
      }
  )};
  auto get_name = [](const VkExtensionProperties& property) {
    return property.extensionName;
  };
  auto unsupported = FindUnsupported<VkExtensionProperties>(
      required, properties, get_name);

  if (unsupported.has_value()) {
    FATAL("Unsupported: " + unsupported.value());
  }
}

void EnsureValidationLayerSupport(const vector<string>& required) {
  std::cout << "Checking validation layer support..." << std::endl << std::endl;

  auto properties {QueryAttribute<VkLayerProperties>(
      [](uint32_t* count, VkLayerProperties* properties) {
        return vkEnumerateInstanceLayerProperties(count, properties);
      }
  )};
  auto get_name = [](const VkLayerProperties& property) {
    return property.layerName;
  };
  auto unsupported = FindUnsupported<VkLayerProperties>(
      required, properties, get_name);

  if (unsupported.has_value()) {
    FATAL("Unsupported: " + unsupported.value());
  }
}

} /* namespace validation */

void DebugCallback::Init(SharedBasicContext context,
                         VkDebugUtilsMessageSeverityFlagsEXT message_severity,
                         VkDebugUtilsMessageTypeFlagsEXT message_type) {
  context_ = std::move(context);
  VkDebugUtilsMessengerCreateInfoEXT create_info{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      message_severity,
      message_type,
      UserCallback,
      /*pUserData=*/nullptr,  // will be passed along to the callback
  };
  static const auto vkCreateDebugUtilsMessengerEXT =
      LoadInstanceFunction<PFN_vkCreateDebugUtilsMessengerEXT>(
          *context_->instance(), "vkCreateDebugUtilsMessengerEXT");
  vkCreateDebugUtilsMessengerEXT(*context_->instance(), &create_info,
                                 context_->allocator(), &callback_);
}

DebugCallback::~DebugCallback() {
  static const auto vkDestroyDebugUtilsMessengerEXT =
      LoadInstanceFunction<PFN_vkDestroyDebugUtilsMessengerEXT>(
          *context_->instance(), "vkDestroyDebugUtilsMessengerEXT");
  vkDestroyDebugUtilsMessengerEXT(*context_->instance(), callback_,
                                  context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
