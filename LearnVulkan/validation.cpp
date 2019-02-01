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
        uint32_t count;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        vector<VkExtensionProperties> extensions{count};
        vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
        unordered_set<string> availableExtensions{count};
        
        cout << "Available instance extensions:" << endl;
        for (const auto& extension : extensions) {
            cout << "\t" << extension.extensionName << endl;
            availableExtensions.insert(extension.extensionName);
        }
        cout << endl;
        
        cout << "Required instance extensions:" << endl;
        for (const auto& extension : requiredExtensions)
            cout << "\t" << extension << endl;
        cout << endl;
        
        checkRequirements(availableExtensions, requiredExtensions);
    }
    
    void checkDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice,
                                     const vector<string>& requiredExtensions) {
        uint32_t count;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
        vector<VkExtensionProperties> extensions{count};
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, extensions.data());
        unordered_set<string> availableExtensions{count};
        
        cout << "Available device extensions:" << endl;
        for (const auto& extension : extensions) {
            cout << "\t" << extension.extensionName << endl;
            availableExtensions.insert(extension.extensionName);
        }
        cout << endl;
        
        cout << "Required device extensions:" << endl;
        for (const auto& extension : requiredExtensions)
            cout << "\t" << extension << endl;
        cout << endl;
        
        checkRequirements(availableExtensions, requiredExtensions);
    }
    
    void checkValidationLayerSupport(const vector<string>& requiredLayers) {
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        vector<VkLayerProperties> layers{count};
        vkEnumerateInstanceLayerProperties(&count, layers.data());
        unordered_set<string> availableLayers{count};
        
        cout << "Available validation layers:" << endl;
        for (const auto& layer : layers) {
            cout << "\t" << layer.layerName << endl;
            availableLayers.insert(layer.layerName);
        }
        cout << endl;
        
        cout << "Required validation layers:" << endl;
        for (const auto& layer : requiredLayers)
            cout << "\t" << layer << endl;
        cout << endl;
        
        checkRequirements(availableLayers, requiredLayers);
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
