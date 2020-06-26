//
//  validation.h
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_VALIDATION_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_VALIDATION_H

#include <vector>

#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
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

// Returns the names of required layers for validation support.
const std::vector<const char*>& GetRequiredLayers();

} /* namespace validation */

// Relays debug messages from graphics drivers back to application.
class DebugCallback {
 public:
  // Specifies messages of which severity and type can trigger debug callbacks.
  struct TriggerCondition {
    TriggerCondition()
        : severity{message_severity::kWarning
                       | message_severity::kError},
          type{message_type::kGeneral
                   | message_type::kValidation
                   | message_type::kPerformance} {}

    VkDebugUtilsMessageSeverityFlagsEXT severity;
    VkDebugUtilsMessageTypeFlagsEXT type;
  };

  DebugCallback(const BasicContext* context,
                const TriggerCondition& trigger_condition);

  // This class is neither copyable nor movable.
  DebugCallback(const DebugCallback&) = delete;
  DebugCallback& operator=(const DebugCallback&) = delete;

  ~DebugCallback();

 private:
  // Pointer to context.
  const BasicContext* context_;

  // Opaque callback object.
  VkDebugUtilsMessengerEXT callback_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_VALIDATION_H */
