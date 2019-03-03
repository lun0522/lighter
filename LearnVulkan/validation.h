//
//  validation.h
//  LearnVulkan
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifdef DEBUG
#ifndef LEARNVULKAN_VALIDATION_H
#define LEARNVULKAN_VALIDATION_H

#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "util.h"

namespace vulkan {

class Application;

namespace MessageSeverity {
  enum Severity {
    kVerbose  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
    kInfo     = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
    kWarning  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
    kError    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
  };
}

namespace MessageType {
  enum Type {
    kGeneral      = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
    kValidation   = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
    kPerformance  = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
  };
}

class DebugCallback {
 public:
  DebugCallback(const Application& app) : app_{app} {}
  void Init(VkDebugUtilsMessageSeverityFlagsEXT message_severity,
            VkDebugUtilsMessageTypeFlagsEXT message_type);
  ~DebugCallback();
  MARK_NOT_COPYABLE_OR_MOVABLE(DebugCallback);

 private:
  const Application& app_;
  VkDebugUtilsMessengerEXT callback_;
};

extern const std::vector<const char*> kValidationLayers;
void CheckInstanceExtensionSupport(const std::vector<std::string>& required);
void CheckValidationLayerSupport(const std::vector<std::string>& required);

} /* namespace vulkan */

#endif /* LEARNVULKAN_VALIDATION_H */
#endif /* DEBUG */
