//
//  main.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/27/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include <iostream>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "utils.hpp"

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
    
    void createInstance();
};

void VulkanApplication::createInstance() {
#ifdef DEBUG
    Utils::checkVulkanSupport();
    Utils::checkExtensionSupport();
    Utils::checkValidationLayerSupport();
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
    
    uint32_t glfwExtensionCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    
#ifdef DEBUG
    createInfo.enabledLayerCount = static_cast<uint32_t>(Utils::validationLayers.size());
    createInfo.ppEnabledLayerNames = Utils::validationLayers.data();
#else
    createInfo.enabledLayerCount = 0;
#endif /* DEBUG */
    
    // the second parameter is a pointer to custom allocator callbacks
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        throw runtime_error{"Failed to create instance"};
}

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
