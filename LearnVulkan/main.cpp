//
//  main.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/27/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include <iostream>
#include <utility>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "validation.hpp"

using namespace std;

class VulkanApplication {
public:
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
    VkDevice device;
    VkPhysicalDevice physicalDevice; // implicitly cleaned up
    VkQueue graphicsQueue; // implicitly cleaned up with physical device
    uint32_t queueFamilyIndex;
    const int WIDTH = 800;
    const int HEIGHT = 600;
    
#ifdef DEBUG
    VkDebugUtilsMessengerEXT callback;
    const vector<const char*> validationLayers{"VK_LAYER_LUNARG_standard_validation"};
#endif /* DEBUG */
    
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Learn Vulkan", nullptr, nullptr);
    }
    
    void initVulkan() {
        createInstance();
#ifdef DEBUG
        Validation::createDebugCallback(
            instance, &callback, nullptr,
            Validation::WARNING | Validation::ERROR,
            Validation::GENERAL | Validation::VALIDATION | Validation::PERFORMANCE);
#endif /* DEBUG */
        pickPhysicalDevice();
        createLogicalDevice();
        getDeviceQueue();
    }
    
    void cleanup() {
#ifdef DEBUG
        Validation::destroyDebugCallback(instance, &callback, nullptr);
#endif /* DEBUG */
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void getDeviceQueue();
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

pair<bool, uint32_t> isDeviceSuitable(const VkPhysicalDevice& device) {
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
    
    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueCount && families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            return {true, i};
    }
    return {false, 0};
}

void VulkanApplication::pickPhysicalDevice() {
    uint32_t count;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (!count) throw runtime_error{"Failed to find GPU with Vulkan support"};
    vector<VkPhysicalDevice> devices{count};
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    
    for (const auto& device : devices) {
        auto res = isDeviceSuitable(device);
        if (res.first) {
            physicalDevice = device;
            queueFamilyIndex = res.second;
            return;
        }
    }
    throw runtime_error{"Failed to find suitable GPU"};
}

void VulkanApplication::createLogicalDevice() {
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority; // required even if only one queue
    
    VkPhysicalDeviceFeatures features{};
    
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
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
}

void VulkanApplication::getDeviceQueue() {
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &graphicsQueue);
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
