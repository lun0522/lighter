//
//  validation.h
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifndef NDEBUG
#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_VALIDATION_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_VALIDATION_H

#include <memory>
#include <string>
#include <vector>

#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

namespace message_severity {

enum Severity {
  kVerbose  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
  kInfo     = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
  kWarning  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
  kError    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
};

} /* namespace MessageSeverity */

namespace message_type {

enum Type {
  kGeneral      = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
  kValidation   = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
  kPerformance  = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
};

} /* namespace MessageType */

namespace validation {

const std::vector<const char*>& layers();
void CheckInstanceExtensionSupport(const std::vector<std::string>& required);
void CheckValidationLayerSupport(const std::vector<std::string>& required);

} /* namespace validation */

class DebugCallback {
 public:
  DebugCallback() = default;

  // This class is neither copyable nor movable
  DebugCallback(const DebugCallback&) = delete;
  DebugCallback& operator=(const DebugCallback&) = delete;

  ~DebugCallback();

  void Init(const std::shared_ptr<Context>& context,
            VkDebugUtilsMessageSeverityFlagsEXT message_severity,
            VkDebugUtilsMessageTypeFlagsEXT message_type);

 private:
  std::shared_ptr<Context> context_;
  VkDebugUtilsMessengerEXT callback_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_VALIDATION_H */
#endif /* !NDEBUG */
