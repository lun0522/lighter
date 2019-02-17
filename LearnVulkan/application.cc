//
//  application.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "application.h"

#include <iostream>
#include <unordered_set>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace vulkan {
    void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->resized() = true;
    }
    
    Application::Application(const string& vert_file,
                             const string& frag_file,
                             uint32_t width,
                             uint32_t height)
    : instance_{}, surface_{*this}, physical_device_{*this},
      device_{*this}, swap_chain_{*this}, render_pass_{*this},
      pipeline_{*this, vert_file, frag_file}, command_buffer_{*this}
#ifdef DEBUG
      , callback_{*this}
#endif /* DEBUG */
    {
        InitWindow(width, height);
        InitVulkan();
    }
    
    void Application::InitWindow(uint32_t width, uint32_t height) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window_ = glfwCreateWindow(width, height, "Learn Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window_, this); // so that we can retrive `this` in callback
        glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    }
    
    VkExtent2D Application::current_extent() const {
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }
    
    void Application::InitVulkan() {
        if (first_time_) {
            instance_.Init();
#ifdef DEBUG
            // relay debug messages back to application
            callback_.init(MessageSeverity::kWarning | MessageSeverity::kError,
                           MessageType::kGeneral | MessageType::kValidation | MessageType::kPerformance);
#endif /* DEBUG */
            surface_.Init();
            physical_device_.Init();
            device_.Init();
            first_time_ = false;
        }
        swap_chain_.init();
        render_pass_.init();
        pipeline_.init();    // fixed and programmable statges
        command_buffer_.init();   // record all operations we want to perform
    }
    
    void Application::MainLoop() {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            if (command_buffer_.drawFrame() != VK_SUCCESS || resized_) {
                resized_ = false;
                Recreate();
            }
        }
        vkDeviceWaitIdle(*device_); // wait for all async operations finish
    }
    
    void Application::Recreate() {
        // do nothing if window is minimized
        int width = 0, height = 0;
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window_, &width, &height);
            glfwWaitEvents();
        }
        
        vkDeviceWaitIdle(*device_);
        Cleanup();
        InitVulkan();
    }
    
    void Application::Cleanup() {
        command_buffer_.cleanup();
        pipeline_.cleanup();
        render_pass_.cleanup();
        swap_chain_.cleanup();
    }
    
    Application::~Application() {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}
