//
//  utils.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifdef DEBUG

#include "utils.hpp"

#include <iostream>
#include <unordered_set>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

using namespace std;

namespace Utils {
    const vector<const char*> validationLayers{"VK_LAYER_LUNARG_standard_validation"};
    
    void checkVulkanSupport() {
        if (glfwVulkanSupported() == GL_FALSE)
            throw runtime_error("Vulkan not supported");
    }
    
    void checkRequirements(const unordered_set<string>& available,
                           const vector<string>& required) {
        for (const auto& req : required)
            if (available.find(req) == available.end())
                throw runtime_error{"Requirement not satisfied: " + req};
    }
    
    void checkExtensionSupport() {
        uint32_t vkExtensionCount;
        vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, nullptr);
        vector<VkExtensionProperties> vkExtensions(vkExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, vkExtensions.data());
        unordered_set<string> availableExtensions(vkExtensionCount);
        
        cout << "Available extensions:" << endl;
        for (const auto& extension : vkExtensions) {
            cout << "\t" << extension.extensionName << endl;
            availableExtensions.insert(extension.extensionName);
        }
        cout << endl;
        
        uint32_t glfwExtensionCount;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        vector<string> requiredExtensions(glfwExtensionCount);
        
        cout << "Required extensions:" << endl;
        for (uint32_t i = 0; i != glfwExtensionCount; ++i) {
            cout << "\t" << glfwExtensions[i] << endl;
            requiredExtensions[i] = glfwExtensions[i];
        }
        cout << endl;
        
        checkRequirements(availableExtensions, requiredExtensions);
    }
    
    void checkValidationLayerSupport() {
        uint32_t vkLayerCount;
        vkEnumerateInstanceLayerProperties(&vkLayerCount, nullptr);
        vector<VkLayerProperties> vkLayers(vkLayerCount);
        vkEnumerateInstanceLayerProperties(&vkLayerCount, vkLayers.data());
        unordered_set<string> availableLayers(vkLayerCount);
        
        cout << "Available validation layers:" << endl;
        for (const auto& layer : vkLayers) {
            cout << "\t" << layer.layerName << endl;
            availableLayers.insert(layer.layerName);
        }
        cout << endl;
        
        vector<string> requiredLayers(validationLayers.size());
        
        cout << "Required validation layers:" << endl;
        for (size_t i = 0; i != validationLayers.size(); ++i) {
            cout << "\t" << validationLayers[i] << endl;
            requiredLayers[i] = validationLayers[i];
        }
        cout << endl;
        
        checkRequirements(availableLayers, requiredLayers);
    }
}

#endif /* DEBUG */
