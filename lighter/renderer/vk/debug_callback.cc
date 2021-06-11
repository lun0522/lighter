//
//  debug_callback.cc
//
//  Created by Pujun Lun on 10/23/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/debug_callback.h"

#include <string>

#include "lighter/common/util.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/strings/str_join.h"

namespace lighter::renderer::vk {
namespace {

// Converts debug_message::severity::Severity to Vulkan native flags.
VkDebugUtilsMessageSeverityFlagsEXT ConvertDebugMessageSeverity(
    uint32_t severity) {
  VkDebugUtilsMessageSeverityFlagsEXT flags = nullflag;
  if (severity & debug_message::severity::INFO) {
    flags |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    flags |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
  }
  if (severity & debug_message::severity::WARNING) {
    flags |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
  }
  if (severity & debug_message::severity::ERROR) {
    flags |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  }
  return flags;
}

// Converts debug_message::type::Type to Vulkan native flags.
VkDebugUtilsMessageTypeFlagsEXT ConvertDebugMessageType(uint32_t type) {
  VkDebugUtilsMessageTypeFlagsEXT flags = nullflag;
  if (type & debug_message::type::GENERAL) {
    flags |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    flags |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
  }
  if (type & debug_message::type::PERFORMANCE) {
    flags |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  }
  return flags;
}

// Converts 'severity' to string.
std::string ToString(VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
  switch (severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      return "VERBOSE";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      return "INFO";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      return "WARNING";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

// Converts 'types' to string.
std::string ToString(VkDebugUtilsMessageTypeFlagsEXT types) {
  std::vector<std::string> type_strings;
  if (types & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
    type_strings.push_back( "GENERAL");
  }
  if (types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
    type_strings.push_back("VALIDATION");
  }
  if (types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
    type_strings.push_back("PERFORMANCE");
  }
  return absl::StrJoin(type_strings, "|");
}

// Returns a callback that prints the message.
VKAPI_ATTR VkBool32 VKAPI_CALL UserCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  const std::string log =
      absl::StrFormat("[DebugCallback] severity %s, types %s",
                      ToString(message_severity), ToString(message_type));
  if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    LOG_ERROR << log;
    LOG_ERROR << callback_data->pMessage;
  } else {
    LOG_INFO << log;
    LOG_INFO << callback_data->pMessage;
  }
  return VK_FALSE;
}

}  // namespace

DebugCallback::DebugCallback(const Context* context,
                             const debug_message::Config& config)
    : context_{*FATAL_IF_NULL(context)} {
  // We may pass data to 'pUserData' which can be retrieved from the callback.
  const VkDebugUtilsMessengerCreateInfoEXT create_info{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = nullflag,
      ConvertDebugMessageSeverity(config.message_severity),
      ConvertDebugMessageType(config.message_type),
      UserCallback,
      .pUserData = nullptr,
  };
  const auto vkCreateDebugUtilsMessengerEXT =
      util::LoadInstanceFunction<PFN_vkCreateDebugUtilsMessengerEXT>(
          *context_.instance(), "vkCreateDebugUtilsMessengerEXT");
  vkCreateDebugUtilsMessengerEXT(*context_.instance(), &create_info,
                                 *context_.host_allocator(), &callback_);
}

const std::vector<const char*>& DebugCallback::GetRequiredLayers() {
  static const auto* validation_layers = new std::vector<const char*>{
      "VK_LAYER_KHRONOS_validation",
  };
  return *validation_layers;
}

const std::vector<const char*>& DebugCallback::GetRequiredExtensions() {
  static const auto* validation_extensions = new std::vector<const char*>{
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
  };
  return *validation_extensions;
}

DebugCallback::~DebugCallback() {
  const auto vkDestroyDebugUtilsMessengerEXT =
      util::LoadInstanceFunction<PFN_vkDestroyDebugUtilsMessengerEXT>(
          *context_.instance(), "vkDestroyDebugUtilsMessengerEXT");
  vkDestroyDebugUtilsMessengerEXT(*context_.instance(), callback_,
                                  *context_.host_allocator());
}

}  // namespace lighter::renderer::vk
