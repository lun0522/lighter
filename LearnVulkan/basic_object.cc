//
//  basic_object.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "basic_object.h"

#include <iostream>
#include <vector>
#include <unordered_set>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "application.h"
#include "swap_chain.h"
#include "validation.h"

namespace vulkan {

using namespace std;

void Instance::Init() {
    if (glfwVulkanSupported() == GL_FALSE)
        throw runtime_error{"Vulkan not supported"};
    
    uint32_t glfw_extension_count;
    const char **glfw_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    
#ifdef DEBUG
    vector<const char*> required_extensions{
        glfw_extensions, glfw_extensions + glfw_extension_count};
    // one extra extension to enable debug report
    required_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    CheckInstanceExtensionSupport(
        {required_extensions.begin(), required_extensions.end()});
    CheckValidationLayerSupport(
        {kValidationLayers.begin(), kValidationLayers.end()});
#endif /* DEBUG */
    
    // [optional]
    // might be useful for the driver to optimize for some graphics engine
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Learn Vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
    
    // [required]
    // tell the driver which global extensions and validation layers to use
    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
#ifdef DEBUG
    instance_info.enabledExtensionCount = CONTAINER_SIZE(required_extensions);
    instance_info.ppEnabledExtensionNames = required_extensions.data();
    instance_info.enabledLayerCount = CONTAINER_SIZE(kValidationLayers);
    instance_info.ppEnabledLayerNames = kValidationLayers.data();
#else
    instanceInfo.enabledExtensionCount = glfw_extension_count;
    instanceInfo.ppEnabledExtensionNames = glfw_extensions;
    instanceInfo.enabledLayerCount = 0;
#endif /* DEBUG */
    
    ASSERT_SUCCESS(vkCreateInstance(&instance_info, nullptr, &instance_),
                   "Failed to create instance");
}

void Surface::Init() {
    const VkInstance& instance = *app_.instance();
    GLFWwindow* window = app_.window();
    ASSERT_SUCCESS(glfwCreateWindowSurface(
                       instance, window, nullptr, &surface_),
                   "Failed to create window surface");
}

Surface::~Surface() {
    const VkInstance& instance = *app_.instance();
    vkDestroySurfaceKHR(instance, surface_, nullptr);
}

bool IsDeviceSuitable(Queues& queues,
                      const VkPhysicalDevice& physical_device,
                      const VkSurfaceKHR& surface) {
    // require swap chain support
    if (!SwapChain::HasSwapChainSupport(surface, physical_device))
        return false;
    
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    cout << "Found device: " << properties.deviceName << endl;
    cout << endl;
    
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physical_device, &features);
    
    auto families{util::QueryAttribute<VkQueueFamilyProperties>(
        [&physical_device]
        (uint32_t* count, VkQueueFamilyProperties* properties) {
            return vkGetPhysicalDeviceQueueFamilyProperties(
                physical_device, count, properties);
        })
    };
    
    // find queue family that holds graphics queue
    auto graphics_support = [](const VkQueueFamilyProperties& family) -> bool {
        return family.queueCount && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
    };
    if (!util::FindFirst<VkQueueFamilyProperties>(
        families, graphics_support, queues.graphics.family_index))
        return false;
    
    // find queue family that holds present queue
    uint32_t index = 0;
    auto present_support = [&physical_device, &surface, index]
        (const VkQueueFamilyProperties& family) mutable -> bool {
        VkBool32 support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device, index++, surface, &support);
        return support;
    };
    if (!util::FindFirst<VkQueueFamilyProperties>(
        families, present_support, queues.present.family_index))
        return false;
    
    return true;
}

void PhysicalDevice::Init() {
    const VkInstance& instance = *app_.instance();
    const VkSurfaceKHR& surface = *app_.surface();
    Queues& queues = app_.queues();
    
    auto devices{util::QueryAttribute<VkPhysicalDevice>(
        [&instance](uint32_t* count, VkPhysicalDevice* physical_device) {
            return vkEnumeratePhysicalDevices(instance, count, physical_device);
        })
    };
    
    for (const auto& candidate : devices) {
        if (IsDeviceSuitable(queues, candidate, surface)) {
            physical_device_ = candidate;
            return;
        }
    }
    throw runtime_error{"Failed to find suitable GPU"};
}

void Device::Init() {
    const VkPhysicalDevice& physical_device = *app_.physical_device();
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
        queue_info.pQueuePriorities = &priority; // always required
        queue_infos.emplace_back(queue_info);
    }
    
    VkPhysicalDeviceFeatures features{};
    
    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pEnabledFeatures = &features;
    device_info.queueCreateInfoCount = CONTAINER_SIZE(queue_infos);
    device_info.pQueueCreateInfos = queue_infos.data();
    device_info.enabledExtensionCount = CONTAINER_SIZE(kSwapChainExtensions);
    device_info.ppEnabledExtensionNames = kSwapChainExtensions.data();
#ifdef DEBUG
    device_info.enabledLayerCount = CONTAINER_SIZE(kValidationLayers);
    device_info.ppEnabledLayerNames = kValidationLayers.data();
#else
    deviceInfo.enabledLayerCount = 0;
#endif /* DEBUG */
    
    ASSERT_SUCCESS(vkCreateDevice(
                       physical_device, &device_info, nullptr, &device_),
                   "Failed to create logical device");
    
    // retrieve queue handles for each queue family
    vkGetDeviceQueue(device_, queues.graphics.family_index, 0,
                     &queues.graphics.queue);
    vkGetDeviceQueue(device_, queues.present.family_index, 0,
                     &queues.present.queue);
}

} /* namespace vulkan */
