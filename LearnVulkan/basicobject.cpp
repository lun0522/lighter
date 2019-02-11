//
//  basicobject.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "basicobject.hpp"

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "application.hpp"
#include "swapchain.hpp"
#include "utils.hpp"
#include "validation.hpp"

namespace VulkanWrappers {
    void Instance::init() {
        if (glfwVulkanSupported() == GL_FALSE)
            throw runtime_error{"Vulkan not supported"};
        
        uint32_t glfwExtensionCount;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        
#ifdef DEBUG
        vector<const char*> requiredExtensions{glfwExtensions, glfwExtensions + glfwExtensionCount};
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // enable debug report
        checkInstanceExtensionSupport({requiredExtensions.begin(), requiredExtensions.end()});
        checkValidationLayerSupport({validationLayers.begin(), validationLayers.end()});
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
        
        ASSERT_SUCCESS(vkCreateInstance(&instanceInfo, nullptr, &instance),
                       "Failed to create instance");
    }
    
    void Surface::init() {
        ASSERT_SUCCESS(glfwCreateWindowSurface(*app.getInstance(), app.getWindow(), nullptr, &surface),
                       "Failed to create window surface");
    }
    
    void Surface::cleanup() {
        vkDestroySurfaceKHR(*app.getInstance(), surface, nullptr);
    }
    
    bool isDeviceSuitable(Queues &queues,
                          const PhysicalDevice &phyDevice,
                          const Surface &surface) {
        // require swap chain support
        if (!SwapChain::hasSwapChainSupport(surface, phyDevice))
            return false;
        
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(*phyDevice, &properties);
        cout << "Found device: " << properties.deviceName << endl;
        cout << endl;
        
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(*phyDevice, &features);
        
        // find queue family that holds graphics queue
        auto families{Utils::queryAttribute<VkQueueFamilyProperties>
            ([&phyDevice](uint32_t *count, VkQueueFamilyProperties *properties) {
                return vkGetPhysicalDeviceQueueFamilyProperties(*phyDevice, count, properties);
            })
        };
        
        bool found = false;
        for (uint32_t i = 0; i < families.size(); ++i) {
            if (families[i].queueCount && families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                queues.graphicsFamily = i;
                found = true;
                break;
            }
        }
        if (!found) return false;
        
        for (uint32_t i = 0; i < families.size(); ++i) {
            if (families[i].queueCount) {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(*phyDevice, i, *surface, &presentSupport);
                if (presentSupport) {
                    queues.presentFamily = i;
                    return true;
                }
            }
        }
        return false;
    }
    
    void PhysicalDevice::init() {
        auto devices{Utils::queryAttribute<VkPhysicalDevice>
            ([this](uint32_t *count, VkPhysicalDevice *phyDevices) {
                return vkEnumeratePhysicalDevices(*app.getInstance(), count, phyDevices);
            })
        };
        
        for (const auto &candidate : devices) {
            if (isDeviceSuitable(app.getQueues(), {app, candidate}, app.getSurface())) {
                phyDevice = candidate;
                return;
            }
        }
        throw runtime_error{"Failed to find suitable GPU"};
    }
    
    void Device::init() {
        Queues &queues = app.getQueues();
        vector<VkDeviceQueueCreateInfo> queueInfos{};
        // graphics queue and present queue might be the same
        unordered_set<uint32_t> queueFamilies{queues.graphicsFamily, queues.presentFamily};
        
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
        
        ASSERT_SUCCESS(vkCreateDevice(*app.getPhysicalDevice(), &deviceInfo, nullptr, &device),
                       "Failed to create logical device");
        
        // retrieve queue handles for each queue family
        vkGetDeviceQueue(device, queues.graphicsFamily, 0, &queues.graphicsQueue);
        vkGetDeviceQueue(device, queues.presentFamily, 0, &queues.presentQueue);
    }
}
