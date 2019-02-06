//
//  application.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "application.hpp"

#include <iostream>
#include <unordered_set>
#include <vector>

#include "utils.hpp"

using namespace std;

#ifdef DEBUG
using Validation::validationLayers;
#endif /* DEBUG */

void VulkanApplication::createInstance() {
#ifdef DEBUG
    if (glfwVulkanSupported() == GL_FALSE)
        throw runtime_error{"Vulkan not supported"};
#endif /* DEBUG */
    
    uint32_t glfwExtensionCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
#ifdef DEBUG
    vector<const char*> requiredExtensions{glfwExtensions, glfwExtensions + glfwExtensionCount};
    requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // enable debug report
    Validation::checkInstanceExtensionSupport({requiredExtensions.begin(), requiredExtensions.end()});
    Validation::checkValidationLayerSupport({validationLayers.begin(), validationLayers.end()});
#endif /* DEBUG */
    
    // optional. might be useful for the driver to optimize for some graphics engine
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Learn Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    // required. tell the driver which global extensions and validation layers to use
    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
#ifdef DEBUG
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    instanceInfo.ppEnabledExtensionNames = requiredExtensions.data();
    instanceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    instanceInfo.ppEnabledLayerNames = validationLayers.data();
#else
    instanceInfo.enabledExtensionCount = glfwExtensionCount;
    instanceInfo.ppEnabledExtensionNames = glfwExtensions;
    instanceInfo.enabledLayerCount = 0;
#endif /* DEBUG */
    
    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS)
        throw runtime_error{"Failed to create instance"};
}

void VulkanApplication::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw runtime_error{"Failed to create window surface"};
}

bool isDeviceSuitable(const VkPhysicalDevice& device,
                      const VkSurfaceKHR& surface,
                      VulkanApplication::QueueFamilyIndices& indices) {
    // require swap chain support
    if (!SwapChain::hasSwapChainSupport(surface, device))
        return false;
    
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    cout << "Found device: " << properties.deviceName << endl;
    cout << endl;
    
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);
    
    // find queue family that holds graphics queue
    auto families{Utils::queryAttribute<VkQueueFamilyProperties>
        ([&device](uint32_t *count, VkQueueFamilyProperties *properties) {
            return vkGetPhysicalDeviceQueueFamilyProperties(device, count, properties);
        })
    };
    
    VulkanApplication::QueueFamilyIndices candidates;
    bool found = false;
    
    for (uint32_t i = 0; i < families.size(); ++i) {
        if (families[i].queueCount && families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            candidates.graphicsFamily = i;
            found = true;
            break;
        }
    }
    if (!found) return false;
    
    for (uint32_t i = 0; i < families.size(); ++i) {
        if (families[i].queueCount) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                candidates.presentFamily = i;
                indices = candidates;
                return true;
            }
        }
    }
    return false;
}

void VulkanApplication::pickPhysicalDevice() {
    auto devices{Utils::queryAttribute<VkPhysicalDevice>
        ([this](uint32_t *count, VkPhysicalDevice *devices) {
            return vkEnumeratePhysicalDevices(instance, count, devices);
        })
    };
    
    for (const auto& candidate : devices) {
        if (isDeviceSuitable(candidate, surface, indices)) {
            physicalDevice = candidate;
            return;
        }
    }
    throw runtime_error{"Failed to find suitable GPU"};
}

void VulkanApplication::createLogicalDevice() {
    vector<VkDeviceQueueCreateInfo> queueInfos{};
    // graphics queue and present queue might be the same
    unordered_set<uint32_t> queueFamilies{indices.graphicsFamily, indices.presentFamily};
    
    float priority = 1.0f;
    for (uint32_t queueFamily : queueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority; // required even if only one queue
        queueInfos.push_back(queueInfo);
    }
    
    VkPhysicalDeviceFeatures features{};
    
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.pEnabledFeatures = &features;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(SwapChain::requiredExtensions.size());
    deviceInfo.ppEnabledExtensionNames = SwapChain::requiredExtensions.data();
#ifdef DEBUG
    deviceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    deviceInfo.ppEnabledLayerNames = validationLayers.data();
#else
    deviceInfo.enabledLayerCount = 0;
#endif /* DEBUG */
    
    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS)
        throw runtime_error{"Failed to create logical device"};
    
    // retrieve queue handles for each queue family
    vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
}

void VulkanApplication::createSwapChain() {
    swapChain = new SwapChain{surface, device, physicalDevice, {width, height},
        {indices.graphicsFamily, indices.presentFamily}};
}

namespace {
    VkShaderModule createShaderModule(const VkDevice& device, const vector<char>& code) {
        VkShaderModuleCreateInfo shaderModuleInfo{};
        shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleInfo.codeSize = code.size();
        shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        
        VkShaderModule shaderModule{};
        if (vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule) != VK_SUCCESS)
            throw runtime_error{"Failed to create shader module"};
        
        return shaderModule;
    }
}

void VulkanApplication::createGraphicsPipeline() {
    vector<char> vertCode = Utils::readFile("triangle.vert.spv");
    vector<char> fragCode = Utils::readFile("triangle.frag.spv");
    
    VkShaderModule vertShaderModule = createShaderModule(device, vertCode);
    VkShaderModule fragShaderModule = createShaderModule(device, fragCode);
    
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main"; // entry point of this shader
    // may use .pSpecializationInfo to specify shader constants
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main"; // entry point of this shader
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStageInfo,
        fragShaderStageInfo,
    };
    
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}
