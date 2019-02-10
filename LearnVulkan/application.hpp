//
//  application.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef application_hpp
#define application_hpp

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "pipeline.hpp"
#include "renderpass.hpp"
#include "swapchain.hpp"
#include "validation.hpp"

using VulkanWrappers::Pipeline;
using VulkanWrappers::RenderPass;
using VulkanWrappers::SwapChain;

class VulkanApplication {
public:
    struct QueueFamilyIndices {
        uint32_t graphicsFamily;            // it is possible that one queue can render images
        uint32_t presentFamily;             // while another one can present them to window system
    };
    
private:
    GLFWwindow *window;
    uint32_t width, height;
    VkInstance instance;
    VkSurfaceKHR surface;                   // backed by window created by GLFW, affects device selection
    VkDevice device;
    VkPhysicalDevice physicalDevice;        // implicitly cleaned up
    VkQueue graphicsQueue;                  // implicitly cleaned up with physical device
    VkQueue presentQueue;
    QueueFamilyIndices indices;
    RenderPass *renderPass;
    SwapChain *swapChain;
    Pipeline *pipeline;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers; // implicitly cleaned up with command pool
    VkSemaphore imageAvailableSema;
    VkSemaphore renderFinishedSema;
    
#ifdef DEBUG
    VkDebugUtilsMessengerEXT callback;
#endif /* DEBUG */
    
public:
    VulkanApplication(uint32_t width = 800, uint32_t height = 600)
    : width{width}, height{height} {
        initWindow();
        initVulkan();
    }
    
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device); // wait for all async operations finish
    }
    
    ~VulkanApplication() {
        cleanup();
    }
    
private:
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
        createRenderPass();                 // specify how to use color and depth buffers
        createGraphicsPipeline();           // fixed and programmable parts
        createCommandBuffers();             // record all operations we want to perform in command buffers
        createSemaphores();                 // sync draw commands and presentation
    }
    
    void cleanup() {
#ifdef DEBUG
        Validation::destroyDebugCallback(instance, &callback, nullptr);
#endif /* DEBUG */
        delete renderPass;
        delete swapChain;
        delete pipeline;
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroySemaphore(device, imageAvailableSema, nullptr);
        vkDestroySemaphore(device, renderFinishedSema, nullptr);
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
    void createRenderPass();
    void createGraphicsPipeline();
    void createCommandBuffers();
    void createSemaphores();
    void drawFrame();
};

#endif /* application_hpp */
