//
//  validation.cc
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifdef DEBUG

#include "jessie_steamer/wrapper/vulkan/validation.h"

#include <iostream>
#include <stdexcept>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/context.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::cout;
using std::endl;
using std::string;
using std::vector;

VKAPI_ATTR VkBool32 VKAPI_CALL UserCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  // if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
  cout << "Validation layer: " << callback_data->pMessage << endl;
  return VK_FALSE;
}

template<typename FuncType>
FuncType LoadFunction(SharedContext context, const string& func_name) {
  auto func = reinterpret_cast<FuncType>(
      vkGetInstanceProcAddr(*context->instance(), func_name.c_str()));
  if (!func) {
    throw std::runtime_error{"Failed to load: " + func_name};
  }
  return func;
}

} /* namespace */

const vector<const char*> kValidationLayers{
    "VK_LAYER_LUNARG_standard_validation",
};

void DebugCallback::Init(SharedContext context,
                         VkDebugUtilsMessageSeverityFlagsEXT message_severity,
                         VkDebugUtilsMessageTypeFlagsEXT message_type) {
  context_ = context;
  VkDebugUtilsMessengerCreateInfoEXT create_info{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      /*pNext=*/nullptr,
      util::nullflag,
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

void CheckInstanceExtensionSupport(const vector<string>& required) {
  cout << "Checking instance extension support..." << endl << endl;

  auto properties {util::QueryAttribute<VkExtensionProperties>(
      [](uint32_t* count, VkExtensionProperties* properties) {
        return vkEnumerateInstanceExtensionProperties(
            nullptr, count, properties);
      }
  )};
  auto get_name = [](const VkExtensionProperties& property) {
    return property.extensionName;
  };
  util::CheckSupport<VkExtensionProperties>(required, properties, get_name);
}

void CheckValidationLayerSupport(const vector<string>& required) {
  cout << "Checking validation layer support..." << endl << endl;

  auto properties {util::QueryAttribute<VkLayerProperties>(
      [](uint32_t* count, VkLayerProperties* properties) {
        return vkEnumerateInstanceLayerProperties(count, properties);
      }
  )};
  auto get_name = [](const VkLayerProperties& property) {
    return property.layerName;
  };
  util::CheckSupport<VkLayerProperties>(required, properties, get_name);
}

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* DEBUG */
