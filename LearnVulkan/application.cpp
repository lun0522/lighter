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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace VulkanWrappers {
    void framebufferResizeCallback(GLFWwindow *window, int width, int height) {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->frameBufferResized = true;
    }
    
    Application::Application(const string &vertFile,
                             const string &fragFile,
                             uint32_t width,
                             uint32_t height)
    : instance{}, surface{*this}, phyDevice{*this},
      device{*this}, swapChain{*this}, renderPass{*this},
      pipeline{*this, vertFile, fragFile}, cmdBuffer{*this}
#ifdef DEBUG
      , callback{*this}
#endif /* DEBUG */
    {
        initWindow(width, height);
        initVulkan();
    }
    
    void Application::initWindow(uint32_t width, uint32_t height) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, "Learn Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this); // so that we can retrive `this` in callback
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }
    
    VkExtent2D Application::getCurrentExtent() const {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }
    
    void Application::initVulkan() {
        if (firstTime) {
            instance.init();
#ifdef DEBUG
            // relay debug messages back to application
            callback.init(Debug::WARNING | Debug::ERROR,
                          Debug::GENERAL | Debug::VALIDATION | Debug::PERFORMANCE);
#endif /* DEBUG */
            surface.init();
            phyDevice.init();
            device.init();
            firstTime = false;
        }
        swapChain.init();   // queue of images to present to screen
        renderPass.init();  // specify how to use color and depth buffers
        pipeline.init();    // fixed and programmable statges
        cmdBuffer.init();   // record all operations we want to perform
    }
    
    void Application::mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            if (cmdBuffer.drawFrame() != VK_SUCCESS || frameBufferResized) {
                frameBufferResized = false;
                recreate();
            }
        }
        vkDeviceWaitIdle(*device); // wait for all async operations finish
    }
    
    void Application::recreate() {
        // do nothing if window is minimized
        int width = 0, height = 0;
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
        
        vkDeviceWaitIdle(*device);
        cleanup();
        initVulkan();
    }
    
    void Application::cleanup() {
        cmdBuffer.cleanup();
        pipeline.cleanup();
        renderPass.cleanup();
        swapChain.cleanup();
    }
    
    Application::~Application() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}
