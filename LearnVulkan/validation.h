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

namespace vulkan {

using std::string;
using std::vector;
class Application;

namespace MessageSeverity {
    enum Severity {
        kVerbose    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
        kInfo       = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        kWarning    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        kError      = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    };
}

namespace MessageType {
    enum Type {
        kGeneral        = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
        kValidation     = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        kPerformance    = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    };
}

class DebugCallback {
    const Application& app_;
    VkDebugUtilsMessengerEXT callback_;
public:
    DebugCallback(const Application& app) : app_{app} {}
    void Init(int message_severity,
              int message_type);
    ~DebugCallback();
};

extern const vector<const char*> kValidationLayers;
void CheckInstanceExtensionSupport(const vector<string>& required);
void CheckValidationLayerSupport(const vector<string>& required);

} /* namespace vulkan */

#endif /* LEARNVULKAN_VALIDATION_H */
#endif /* DEBUG */
