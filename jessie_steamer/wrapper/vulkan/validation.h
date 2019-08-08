//
//  validation.h
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_VALIDATION_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_VALIDATION_H

#include <memory>
#include <string>
#include <vector>

#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// Forward declarations.
class BasicContext;

namespace message_severity {

// Bridges VK_DEBUG_UTILS_MESSAGE_SEVERITY.
enum Severity {
  kVerbose  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
  kInfo     = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
  kWarning  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
  kError    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
};

} /* namespace message_severity */

namespace message_type {

// Bridges VK_DEBUG_UTILS_MESSAGE_TYPE.
enum Type {
  kGeneral      = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
  kValidation   = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
  kPerformance  = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
};

} /* namespace message_type */

namespace validation {

// Returns names of layers that should be enabled for validation support.
const std::vector<const char*>& GetValidationLayers();

// Checks support for 'required' extensions, and throws a runtime exception
// if any of them is not supported.
void CheckInstanceExtensionSupport(const std::vector<std::string>& required);

// Checks support for 'required' layers, and throws a runtime exception if any
// of them is not supported.
void CheckValidationLayerSupport(const std::vector<std::string>& required);

} /* namespace validation */

// Relays debug messages from graphics drivers back to application.
// We may determine the severity and type of messages that we care about.
class DebugCallback {
 public:
  DebugCallback() = default;

  // This class is neither copyable nor movable.
  DebugCallback(const DebugCallback&) = delete;
  DebugCallback& operator=(const DebugCallback&) = delete;

  ~DebugCallback();

  // Initializes a callback that will be triggered by messages of
  // 'message_severity' and 'message_type'.
  void Init(std::shared_ptr<BasicContext> context,
            VkDebugUtilsMessageSeverityFlagsEXT message_severity,
            VkDebugUtilsMessageTypeFlagsEXT message_type);

 private:
  // Pointer to context.
  std::shared_ptr<BasicContext> context_;

  // Opaque callback object.
  VkDebugUtilsMessengerEXT callback_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_VALIDATION_H */
