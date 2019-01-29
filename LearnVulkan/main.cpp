//
//  main.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/27/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include <iostream>
#include <unordered_set>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "validation.hpp"

using namespace std;
#ifdef DEBUG
using Validation::validationLayers;
#endif /* DEBUG */

class VulkanApplication {
public:
    struct QueueFamilyIndices {
        uint32_t graphicsFamily;            // it is possible that one queue can render images
        uint32_t presentFamily;             // while another one can present them to window system
    };
    
    VulkanApplication() {
        initWindow();
        initVulkan();
    }
    
    void mainLoop() {
        while (!glfwWindowShouldClose(window))
            glfwPollEvents();
    }
    
    ~VulkanApplication() {
        cleanup();
    }
private:
    GLFWwindow *window;
    VkInstance instance;
    VkSurfaceKHR surface;                   // backed by window created by GLFW, affects device selection
    VkDevice device;
    VkPhysicalDevice physicalDevice;        // implicitly cleaned up
    VkQueue graphicsQueue;                  // implicitly cleaned up with physical device
    VkQueue presentQueue;
    QueueFamilyIndices indices;
    const int WIDTH = 800;
    const int HEIGHT = 600;
    
#ifdef DEBUG
    VkDebugUtilsMessengerEXT callback;
#endif /* DEBUG */
    
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Learn Vulkan", nullptr, nullptr);
    }
    
    void initVulkan() {
        createInstance();                   // establish connection with Vulkan library
#ifdef DEBUG
        Validation::createDebugCallback(    // relay debug messages back to application
            instance, &callback, nullptr,
            Validation::WARNING | Validation::ERROR,
            Validation::GENERAL | Validation::VALIDATION | Validation::PERFORMANCE);
#endif /* DEBUG */
        createSurface();                    // interface with window system (not needed for off-screen rendering)
        pickPhysicalDevice();               // select graphics card to use
        createLogicalDevice();              // interface with physical device
    }
    
    void cleanup() {
#ifdef DEBUG
        Validation::destroyDebugCallback(instance, &callback, nullptr);
#endif /* DEBUG */
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
};

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
    Validation::checkExtensionSupport({requiredExtensions.begin(), requiredExtensions.end()});
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
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
#ifdef DEBUG
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
#else
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;
#endif /* DEBUG */
    
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        throw runtime_error{"Failed to create instance"};
}

void VulkanApplication::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw runtime_error{"Failed to create window surface"};
}

pair<bool, VulkanApplication::QueueFamilyIndices> isDeviceSuitable(const VkPhysicalDevice& device,
                                                                   const VkSurfaceKHR& surface) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    cout << "Found device: " << properties.deviceName << endl;
    cout << endl;
    
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);
    
    // find queue family that holds graphics queue
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    vector<VkQueueFamilyProperties> families{count};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    
    VulkanApplication::QueueFamilyIndices indices;
    bool found = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueCount && families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
            found = true;
            break;
        }
    }
    if (!found) return {false, {}};
    
    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueCount) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
                return {true, indices};
            }
        }
    }
    return {false, {}};
}

void VulkanApplication::pickPhysicalDevice() {
    uint32_t count;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (!count) throw runtime_error{"Failed to find GPU with Vulkan support"};
    vector<VkPhysicalDevice> devices{count};
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    
    for (const auto& candidate : devices) {
        auto res = isDeviceSuitable(candidate, surface);
        if (res.first) {
            physicalDevice = candidate;
            indices = res.second;
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
    deviceInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.pEnabledFeatures = &features;
    deviceInfo.enabledExtensionCount = 0;
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

int main(int argc, const char * argv[]) {
    try {
        VulkanApplication app{};
        app.mainLoop();
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
