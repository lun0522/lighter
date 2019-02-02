//
//  main.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/27/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "validation.hpp"

using namespace std;

using Validation::swapChainExtensions;
#ifdef DEBUG
using Validation::validationLayers;
#endif /* DEBUG */

class VulkanApplication {
public:
    const uint32_t width, height;
    
    struct QueueFamilyIndices {
        uint32_t graphicsFamily;            // it is possible that one queue can render images
        uint32_t presentFamily;             // while another one can present them to window system
    };
    
    struct SwapChainSupport {
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        vector<VkSurfaceFormatKHR> surfaceFormats;
        vector<VkPresentModeKHR> presentModes;
    };
    
    VulkanApplication(uint32_t width = 800, uint32_t height = 600)
        : width{width}, height{height} {
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
    VkSwapchainKHR swapChain;
    vector<VkImage> swapChainImages;        // implicitly cleaned up with swap chain
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    
#ifdef DEBUG
    VkDebugUtilsMessengerEXT callback;
#endif /* DEBUG */
    
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, "Learn Vulkan", nullptr, nullptr);
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
        createSwapChain();                  // queue of images to present to screen
    }
    
    void cleanup() {
#ifdef DEBUG
        Validation::destroyDebugCallback(instance, &callback, nullptr);
#endif /* DEBUG */
        vkDestroySwapchainKHR(device, swapChain, nullptr);
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
    void createSwapChain();
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

bool hasSwapChainSupport(const VkPhysicalDevice& device,
                         const VkSurfaceKHR& surface) {
    try {
        Validation::checkDeviceExtensionSupport(
            device, {swapChainExtensions.begin(), swapChainExtensions.end()});
    } catch (const exception& e) {
        return false;
    }
    
    // physical device may support swap chain but maybe not compatible with window system
    // so we need to query details
    uint32_t formatCount, modeCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, nullptr);
    return formatCount != 0 && modeCount != 0;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& availableFormats) {
    // if surface has no preferred format, we can choose any format
    if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
        return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    
    for (const auto& candidate : availableFormats) {
        if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM &&
            candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return candidate;
    }
    
    // if our preferred format is not supported, simply choose the first one
    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR>& availableModes) {
    // FIFO mode is guaranteed to be available, but not properly supported by some drivers
    // we will prefer IMMEDIATE mode over it
    VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
    
    for (const auto& candidate : availableModes) {
        if (candidate == VK_PRESENT_MODE_MAILBOX_KHR) {
            return candidate;
        } else if (candidate == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            bestMode = candidate;
        }
    }
    
    return bestMode;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                            VkExtent2D currentExtent) {
    
    if (capabilities.currentExtent.width != numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        currentExtent.width = max(capabilities.minImageExtent.width, currentExtent.width);
        currentExtent.width = min(capabilities.maxImageExtent.width, currentExtent.width);
        currentExtent.height = max(capabilities.minImageExtent.height, currentExtent.height);
        currentExtent.height = min(capabilities.maxImageExtent.height, currentExtent.height);
        return currentExtent;
    }
}

bool isDeviceSuitable(const VkPhysicalDevice& device,
                      const VkSurfaceKHR& surface,
                      VulkanApplication::QueueFamilyIndices& indices) {
    // require swap chain support
    if (!hasSwapChainSupport(device, surface))
        return false;
    
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
    
    VulkanApplication::QueueFamilyIndices candidates;
    
    bool found = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueCount && families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            candidates.graphicsFamily = i;
            found = true;
            break;
        }
    }
    if (!found) return false;
    
    for (uint32_t i = 0; i < count; ++i) {
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
    uint32_t count;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    vector<VkPhysicalDevice> devices{count};
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    
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
    deviceInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.pEnabledFeatures = &features;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(swapChainExtensions.size());
    deviceInfo.ppEnabledExtensionNames = swapChainExtensions.data();
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
    VulkanApplication::SwapChainSupport support;
    
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &support.surfaceCapabilities);
    
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    support.surfaceFormats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, support.surfaceFormats.data());
    
    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr);
    support.presentModes.resize(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, support.presentModes.data());
    
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.surfaceFormats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
    VkExtent2D extent = chooseSwapExtent(support.surfaceCapabilities, {width, height});
    
    // how many images we want to have in swap chain
    uint32_t imageCount = support.surfaceCapabilities.minImageCount + 1;
    if (support.surfaceCapabilities.maxImageCount > 0) // can be 0 if no maximum
        imageCount = min(support.surfaceCapabilities.maxImageCount, imageCount);
    
    VkSwapchainCreateInfoKHR swapChainInfo{};
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface = surface;
    swapChainInfo.minImageCount = imageCount;
    swapChainInfo.imageFormat = surfaceFormat.format;
    swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainInfo.imageExtent = extent;
    swapChainInfo.imageArrayLayers = 1;
    swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // can be different for post-processing
    swapChainInfo.preTransform = support.surfaceCapabilities.currentTransform; // may apply transformations
    swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // may change alpha channel
    swapChainInfo.presentMode = presentMode;
    swapChainInfo.clipped = VK_TRUE; // don't care about color of pixels obscured
    swapChainInfo.oldSwapchain = VK_NULL_HANDLE;
    
    if (indices.graphicsFamily != indices.presentFamily) {
        // specify which queue families will share access to images
        uint32_t familyIndices[]{indices.graphicsFamily, indices.presentFamily};
        swapChainInfo.queueFamilyIndexCount = 2;
        swapChainInfo.pQueueFamilyIndices = familyIndices;
        swapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    } else {
        // no need to specify queue families
        // we will draw on images in swap chain from graphics queue
        // and submit on presentation queue
        swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    if (vkCreateSwapchainKHR(device, &swapChainInfo, nullptr, &swapChain) != VK_SUCCESS)
        throw runtime_error{"Failed to create swap chain"};
    
    // image count might be different since we only set a minimum
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

int main(int argc, const char* argv[]) {
    try {
        VulkanApplication app{};
        app.mainLoop();
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
