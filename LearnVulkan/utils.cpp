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

#include <vulkan/vulkan.hpp>

using namespace std;

namespace Utils {
    void checkRequirements(const unordered_set<string>& available,
                           const vector<string>& required) {
        for (const auto& req : required)
            if (available.find(req) == available.end())
                throw runtime_error{"Requirement not satisfied: " + req};
    }
    
    void checkExtensionSupport(const vector<string>& requiredExtensions) {
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
        
        cout << "Required extensions:" << endl;
        for (const auto& extension : requiredExtensions)
            cout << "\t" << extension << endl;
        cout << endl;
        
        checkRequirements(availableExtensions, requiredExtensions);
    }
    
    void checkValidationLayerSupport(const vector<string>& requiredLayers) {
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
        
        cout << "Required validation layers:" << endl;
        for (const auto& layer : requiredLayers)
            cout << "\t" << layer << endl;
        cout << endl;
        
        checkRequirements(availableLayers, requiredLayers);
    }
}

#endif /* DEBUG */
