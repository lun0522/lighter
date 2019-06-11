//
//  validation.cc
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/validation.h"

#include <iostream>
#include <stdexcept>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

namespace util = common::util;
using std::string;
using std::runtime_error;
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

template<typename FuncType>
FuncType LoadFunction(const SharedBasicContext& context,
                      const string& func_name) {
  auto func = reinterpret_cast<FuncType>(
      vkGetInstanceProcAddr(*context->instance(), func_name.c_str()));
  if (!func) {
    throw runtime_error{"Failed to load: " + func_name};
  }
  return func;
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

  auto properties {util::QueryAttribute<VkExtensionProperties>(
      [](uint32_t* count, VkExtensionProperties* properties) {
        return vkEnumerateInstanceExtensionProperties(
            nullptr, count, properties);
      }
  )};
  auto get_name = [](const VkExtensionProperties& property) {
    return property.extensionName;
  };
  auto unsupported = util::FindUnsupported<VkExtensionProperties>(
      required, properties, get_name);

  if (unsupported.has_value()) {
    throw runtime_error{"Unsupported: " + unsupported.value()};
  }
}

void EnsureValidationLayerSupport(const vector<string>& required) {
  std::cout << "Checking validation layer support..." << std::endl << std::endl;

  auto properties {util::QueryAttribute<VkLayerProperties>(
      [](uint32_t* count, VkLayerProperties* properties) {
        return vkEnumerateInstanceLayerProperties(count, properties);
      }
  )};
  auto get_name = [](const VkLayerProperties& property) {
    return property.layerName;
  };
  auto unsupported = util::FindUnsupported<VkLayerProperties>(
      required, properties, get_name);

  if (unsupported.has_value()) {
    throw runtime_error{"Unsupported: " + unsupported.value()};
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

  auto func = LoadFunction<PFN_vkCreateDebugUtilsMessengerEXT>(
      context_, "vkCreateDebugUtilsMessengerEXT");
  func(*context_->instance(), &create_info, context_->allocator(), &callback_);
}

DebugCallback::~DebugCallback() {
  auto func = LoadFunction<PFN_vkDestroyDebugUtilsMessengerEXT>(
      context_, "vkDestroyDebugUtilsMessengerEXT");
  func(*context_->instance(), callback_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
