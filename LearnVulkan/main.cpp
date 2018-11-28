//
//  main.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 11/27/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include <stdlib.h>
#include <iostream>

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
    
    void initWindow(int width = 800, int height = 600) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, "Learn Vulkan", nullptr, nullptr);
    }
    
    void initVulkan() {
        createInstance();
    }
    
    void createInstance() {
        if (glfwVulkanSupported() == GL_FALSE)
            throw runtime_error("Vulkan not supported");
        
        uint32_t glfwExtensionCount;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        
        // optional. might be useful for the driver to optimize for some graphics engine
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Learn Vulkan";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_VERSION_1_0;
        
        // non-optional. tell the driver which global extensions and validation layers to use
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        createInfo.enabledLayerCount = 0;
        
        // the second parameter is a pointer to custom allocator callbacks
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
            throw runtime_error("Failed to create instance");
    }
    
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }
    
    void cleanup() {
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
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
