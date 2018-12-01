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
    void checkExtensionSupport(const std::vector<std::string>& requiredExtensions);
    void checkValidationLayerSupport(const std::vector<std::string>& requiredLayers);
    void createDebugCallback(const VkInstance& instance,
                             VkDebugUtilsMessengerEXT* pCallback,
                             const VkAllocationCallbacks* pAllocator);
    void destroyDebugCallback(const VkInstance& instance,
                              const VkDebugUtilsMessengerEXT* pCallback,
                              const VkAllocationCallbacks* pAllocator);
}

#endif /* VALIDATION_HPP */
#endif /* DEBUG */
