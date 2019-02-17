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
    using namespace std;
    
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
        const Application &app;
        VkDebugUtilsMessengerEXT callback;
    public:
        DebugCallback(const Application &app) : app{app} {}
        void init(int messageSeverity,
                  int messageType);
        ~DebugCallback();
    };
    
    extern const vector<const char*> validationLayers;
    void checkInstanceExtensionSupport(const vector<string> &requiredExtensions);
    void checkValidationLayerSupport(const vector<string> &requiredLayers);
}

#endif /* LEARNVULKAN_VALIDATION_H */
#endif /* DEBUG */
