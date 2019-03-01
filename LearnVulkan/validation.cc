//
//  validation.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifdef DEBUG

#include "validation.h"

#include <iostream>
#include <unordered_set>

#include "application.h"

using namespace std;

namespace vulkan {

namespace {

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
FuncType LoadFunction(const VkInstance& instance, const string& func_name) {
  auto func = reinterpret_cast<FuncType>(
    vkGetInstanceProcAddr(instance, func_name.c_str()));
  if (!func)
    throw runtime_error{"Failed to load: " + func_name};
  return func;
}

} /* namespace */

const vector<const char*> kValidationLayers{
  "VK_LAYER_LUNARG_standard_validation"};

void DebugCallback::Init(int message_severity,
                         int message_type) {
  VkDebugUtilsMessengerCreateInfoEXT create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.messageSeverity = message_severity;
  create_info.messageType = message_type;
  create_info.pfnUserCallback = UserCallback;
  create_info.pUserData = nullptr; // will be passed along to the callback

  const VkInstance& instance = *app_.instance();
  auto func = LoadFunction<PFN_vkCreateDebugUtilsMessengerEXT>(
    instance, "vkCreateDebugUtilsMessengerEXT");
  func(instance, &create_info, nullptr, &callback_);
}

DebugCallback::~DebugCallback() {
  const VkInstance& instance = *app_.instance();
  auto func = LoadFunction<PFN_vkDestroyDebugUtilsMessengerEXT>(
    instance, "vkDestroyDebugUtilsMessengerEXT");
  func(instance, callback_, nullptr);
}

void CheckInstanceExtensionSupport(const vector<string>& required) {
  cout << "Checking instance extension support..." << endl << endl;

  auto properties {util::QueryAttribute<VkExtensionProperties>(
    [](uint32_t* count, VkExtensionProperties* properties) {
      return vkEnumerateInstanceExtensionProperties(nullptr, count, properties);
    })
  };
  auto get_name = [](const VkExtensionProperties &property) -> const char* {
    return property.extensionName;
  };
  util::CheckSupport<VkExtensionProperties>(required, properties, get_name);
}

void CheckValidationLayerSupport(const vector<string>& required) {
  cout << "Checking validation layer support..." << endl << endl;

  auto properties {util::QueryAttribute<VkLayerProperties>(
    [](uint32_t* count, VkLayerProperties* properties) {
      return vkEnumerateInstanceLayerProperties(count, properties);
    })
  };
  auto get_name = [](const VkLayerProperties& property) -> const char* {
    return property.layerName;
  };
  util::CheckSupport<VkLayerProperties>(required, properties, get_name);
}

} /* namespace vulkan */

#endif /* DEBUG */
