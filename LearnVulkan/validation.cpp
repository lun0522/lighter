//
//  validation.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifdef DEBUG

#include "validation.hpp"

#include <iostream>
#include <unordered_set>

#include "utils.hpp"

namespace Validation {
    const vector<const char*> swapChainExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const vector<const char*> validationLayers{"VK_LAYER_LUNARG_standard_validation"};
    
    void checkRequirements(const unordered_set<string>& available,
                           const vector<string>& required) {
        for (const auto& req : required)
            if (available.find(req) == available.end())
                throw runtime_error{"Requirement not satisfied: " + req};
    }
    
    void checkInstanceExtensionSupport(const vector<string>& requiredExtensions) {
        cout << "Checking instance extension support..." << endl << endl;
        
        const auto enumerate = [](uint32_t *count, VkExtensionProperties *properties) {
            return vkEnumerateInstanceExtensionProperties(nullptr, count, properties);
        };
        
        const auto getName = [](const VkExtensionProperties &property) -> const char* {
            return property.extensionName;
        };
        
        Utils::checkSupport<VkExtensionProperties>(requiredExtensions, enumerate, getName);
    }
    
    void checkDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice,
                                     const vector<string>& requiredExtensions) {
        cout << "Checking device extension support..." << endl << endl;
        
        const auto enumerate = [&](uint32_t *count, VkExtensionProperties *properties) {
            return vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, count, properties);
        };
        
        const auto getName = [](const VkExtensionProperties &property) -> const char* {
            return property.extensionName;
        };
        
        Utils::checkSupport<VkExtensionProperties>(requiredExtensions, enumerate, getName);
    }
    
    void checkValidationLayerSupport(const vector<string>& requiredLayers) {
        cout << "Checking validation layer support..." << endl << endl;
        
        const auto enumerate = [](uint32_t *count, VkLayerProperties *properties) {
            return vkEnumerateInstanceLayerProperties(count, properties);
        };
        
        const auto getName = [](const VkLayerProperties &property) -> const char* {
            return property.layerName;
        };
        
        Utils::checkSupport<VkLayerProperties>(requiredLayers, enumerate, getName);
    }
    
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
         VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
         VkDebugUtilsMessageTypeFlagsEXT messageType,
         const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
         void* pUserData) {
        // if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        cerr << "Validation layer: " << pCallbackData->pMessage << endl;
        return VK_FALSE;
    }
    
    template<typename T>
    T loadFunction(const VkInstance& instance, const char* funcName) {
        auto func = reinterpret_cast<T>(vkGetInstanceProcAddr(instance, funcName));
        if (!func) throw runtime_error{"Failed to load: " + string{funcName}};
        return func;
    }
    
    void createDebugCallback(const VkInstance& instance,
                             VkDebugUtilsMessengerEXT* pCallback,
                             const VkAllocationCallbacks* pAllocator,
                             int messageSeverity,
                             int messageType) {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = messageSeverity;
        createInfo.messageType = messageType;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = nullptr; // will be passed along to the callback
        auto func = loadFunction<PFN_vkCreateDebugUtilsMessengerEXT>
            (instance, "vkCreateDebugUtilsMessengerEXT");
        func(instance, &createInfo, pAllocator, pCallback);
    }
    
    void destroyDebugCallback(const VkInstance& instance,
                              const VkDebugUtilsMessengerEXT* pCallback,
                              const VkAllocationCallbacks* pAllocator) {
        auto func = loadFunction<PFN_vkDestroyDebugUtilsMessengerEXT>
            (instance, "vkDestroyDebugUtilsMessengerEXT");
        func(instance, *pCallback, pAllocator);
    }
}

#endif /* DEBUG */
