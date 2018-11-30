//
//  main.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/27/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include <iostream>
#include <unordered_set>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

using namespace std;

class VulkanApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
private:
    GLFWwindow *window;
    VkInstance instance;
    
    const int WIDTH = 800;
    const int HEIGHT = 600;
#ifdef DEBUG
    const vector<const char*> validationLayers{"VK_LAYER_LUNARG_standard_validation"};
#endif
    
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Learn Vulkan", nullptr, nullptr);
    }
    
    void initVulkan() {
        createInstance();
    }
    
    void mainLoop() {
        while (!glfwWindowShouldClose(window))
            glfwPollEvents();
    }
    
    void cleanup() {
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    
    void createInstance() {
#ifdef DEBUG
        checkVulkanSupport();
        checkExtensionSupport();
        checkValidationLayerSupport();
#endif
        
        uint32_t glfwExtensionCount;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        
        // optional. might be useful for the driver to optimize for some graphics engine
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Learn Vulkan";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        
        // non-optional. tell the driver which global extensions and validation layers to use
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        
#ifdef DEBUG
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
#else
        createInfo.enabledLayerCount = 0;
#endif
        
        // the second parameter is a pointer to custom allocator callbacks
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
            throw runtime_error{"Failed to create instance"};
    }
    
#ifdef DEBUG
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
#endif
};

int main(int argc, const char * argv[]) {
    VulkanApplication app;
    try {
        app.run();
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
