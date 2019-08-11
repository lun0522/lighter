//
//  validation.cc
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/validation.h"

#include <iostream>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

// Returns a callback that simply prints the error reason.
VKAPI_ATTR VkBool32 VKAPI_CALL UserCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  std::cout << "[Validation] " << callback_data->pMessage << std::endl;
  return VK_FALSE;
}

} /* namespace */

namespace validation {

const vector<const char*>& GetRequiredLayers() {
  static vector<const char*>* validation_layers = nullptr;
  if (validation_layers == nullptr) {
    validation_layers = new vector<const char*>{
        "VK_LAYER_LUNARG_standard_validation",
    };
  }
  return *validation_layers;
}

} /* namespace validation */

void DebugCallback::Init(SharedBasicContext context,
                         VkDebugUtilsMessageSeverityFlagsEXT message_severity,
                         VkDebugUtilsMessageTypeFlagsEXT message_type) {
  context_ = std::move(context);
  // We may pass data to 'pUserData' which can be retrieved from the callback.
  VkDebugUtilsMessengerCreateInfoEXT create_info{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      message_severity,
      message_type,
      UserCallback,
      /*pUserData=*/nullptr,
  };
  const auto vkCreateDebugUtilsMessengerEXT =
      LoadInstanceFunction<PFN_vkCreateDebugUtilsMessengerEXT>(
          *context_->instance(), "vkCreateDebugUtilsMessengerEXT");
  vkCreateDebugUtilsMessengerEXT(*context_->instance(), &create_info,
                                 *context_->allocator(), &callback_);
}

DebugCallback::~DebugCallback() {
  const auto vkDestroyDebugUtilsMessengerEXT =
      LoadInstanceFunction<PFN_vkDestroyDebugUtilsMessengerEXT>(
          *context_->instance(), "vkDestroyDebugUtilsMessengerEXT");
  vkDestroyDebugUtilsMessengerEXT(*context_->instance(), callback_,
                                  *context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
