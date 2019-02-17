//
//  basic_object.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "basic_object.h"

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "application.h"
#include "swap_chain.h"
#include "util.h"
#include "validation.h"

namespace vulkan {
    void Instance::Init() {
        if (glfwVulkanSupported() == GL_FALSE)
            throw runtime_error{"Vulkan not supported"};
        
        uint32_t glfw_extension_count;
        const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        
#ifdef DEBUG
        vector<const char*> required_extensions{glfw_extensions, glfw_extensions + glfw_extension_count};
        required_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // enable debug report
        checkInstanceExtensionSupport({required_extensions.begin(), required_extensions.end()});
        checkValidationLayerSupport({validationLayers.begin(), validationLayers.end()});
#endif /* DEBUG */
        
        // optional. might be useful for the driver to optimize for some graphics engine
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Learn Vulkan";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;
        
        // required. tell the driver which global extensions and validation layers to use
        VkInstanceCreateInfo instance_info{};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;
#ifdef DEBUG
        instance_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
        instance_info.ppEnabledExtensionNames = required_extensions.data();
        instance_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        instance_info.ppEnabledLayerNames = validationLayers.data();
#else
        instanceInfo.enabledExtensionCount = glfw_extension_count;
        instanceInfo.ppEnabledExtensionNames = glfw_extensions;
        instanceInfo.enabledLayerCount = 0;
#endif /* DEBUG */
        
        ASSERT_SUCCESS(vkCreateInstance(&instance_info, nullptr, &instance_),
                       "Failed to create instance");
    }
    
    void Surface::Init() {
        ASSERT_SUCCESS(glfwCreateWindowSurface(*app_.instance(), app_.window(), nullptr, &surface_),
                       "Failed to create window surface");
    }
    
    void Surface::Cleanup() {
        vkDestroySurfaceKHR(*app_.instance(), surface_, nullptr);
    }
    
    bool IsDeviceSuitable(Queues& queues,
                          const PhysicalDevice& physical_device,
                          const Surface& surface) {
        // require swap chain support
        if (!SwapChain::hasSwapChainSupport(surface, physical_device))
            return false;
        
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(*physical_device, &properties);
        cout << "Found device: " << properties.deviceName << endl;
        cout << endl;
        
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(*physical_device, &features);
        
        auto families{util::queryAttribute<VkQueueFamilyProperties>
            ([&physical_device](uint32_t* count, VkQueueFamilyProperties* properties) {
                return vkGetPhysicalDeviceQueueFamilyProperties(*physical_device, count, properties);
            })
        };
        
        // find queue family that holds graphics queue
        auto graphics_support = [](const VkQueueFamilyProperties& family) -> bool {
            return family.queueCount && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
        };
        if (!util::findFirst(families, graphics_support, queues.graphics.family_index))
            return false;
        
        // find queue family that holds present queue
        uint32_t index = 0;
        auto present_support = [&physical_device, &surface, index]
            (const VkQueueFamilyProperties& family) mutable -> bool {
            VkBool32 support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(*physical_device, index++, *surface, &support);
            return support;
        };
        if (!util::findFirst(families, present_support, queues.present.family_index))
            return false;
        
        return true;
    }
    
    void PhysicalDevice::Init() {
        auto devices{util::queryAttribute<VkPhysicalDevice>
            ([this](uint32_t* count, VkPhysicalDevice* physical_device) {
                return vkEnumeratePhysicalDevices(*app_.instance(), count, physical_device);
            })
        };
        
        for (const auto& candidate : devices) {
            if (IsDeviceSuitable(app_.queues(), {app_, candidate}, app_.surface())) {
                physical_device_ = candidate;
                return;
            }
        }
        throw runtime_error{"Failed to find suitable GPU"};
    }
    
    void Device::Init() {
        Queues& queues = app_.queues();
        // graphics queue and present queue might be the same
        unordered_set<uint32_t> queue_families{
            queues.graphics.family_index,
            queues.present.family_index
        };
        vector<VkDeviceQueueCreateInfo> queue_infos{};
        
        float priority = 1.0f;
        for (uint32_t queue_family : queue_families) {
            VkDeviceQueueCreateInfo queue_info{};
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = queue_family;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &priority; // required even if only one queue
            queue_infos.emplace_back(queue_info);
        }
        
        VkPhysicalDeviceFeatures features{};
        
        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.pEnabledFeatures = &features;
        device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
        device_info.pQueueCreateInfos = queue_infos.data();
        device_info.enabledExtensionCount = static_cast<uint32_t>(SwapChain::requiredExtensions.size());
        device_info.ppEnabledExtensionNames = SwapChain::requiredExtensions.data();
#ifdef DEBUG
        device_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        device_info.ppEnabledLayerNames = validationLayers.data();
#else
        deviceInfo.enabledLayerCount = 0;
#endif /* DEBUG */
        
        ASSERT_SUCCESS(vkCreateDevice(*app_.physical_device(), &device_info, nullptr, &device_),
                       "Failed to create logical device");
        
        // retrieve queue handles for each queue family
        vkGetDeviceQueue(device_, queues.graphics.family_index, 0, &queues.graphics.queue);
        vkGetDeviceQueue(device_, queues.present.family_index, 0, &queues.present.queue);
    }
}
