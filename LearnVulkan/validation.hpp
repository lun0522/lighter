//
//  validation.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifdef DEBUG
#ifndef VALIDATION_HPP
#define VALIDATION_HPP

#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace Validation {
    enum Severity {
        VERBOSE = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
        INFO    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        WARNING = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        ERROR   = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    };
    enum Type {
        GENERAL     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
        VALIDATION  = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        PERFORMANCE = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    };
    extern const std::vector<const char*> validationLayers;
    
    void checkExtensionSupport(const std::vector<std::string>& requiredExtensions);
    void checkValidationLayerSupport(const std::vector<std::string>& requiredLayers);
    void createDebugCallback(const VkInstance& instance,
                             VkDebugUtilsMessengerEXT* pCallback,
                             const VkAllocationCallbacks* pAllocator,
                             int messageSeverity,
                             int messageType);
    void destroyDebugCallback(const VkInstance& instance,
                              const VkDebugUtilsMessengerEXT* pCallback,
                              const VkAllocationCallbacks* pAllocator);
}

#endif /* VALIDATION_HPP */
#endif /* DEBUG */
