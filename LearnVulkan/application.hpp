//
//  application.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef application_hpp
#define application_hpp

#include <vulkan/vulkan.hpp>

#include "commandbuffer.hpp"
#include "pipeline.hpp"
#include "renderpass.hpp"
#include "swapchain.hpp"
#include "utils.hpp"

class GLFWwindow;

namespace VulkanWrappers {
    class Application {
    public:
        struct Queues {
            VkQueue graphicsQueue;
            VkQueue presentQueue;
            uint32_t graphicsFamily;
            uint32_t presentFamily;
        };
        
        Application(uint32_t width = 800, uint32_t height = 600);
        void mainLoop();
        ~Application();
        
        VkExtent2D getExtent() const { return {width, height}; }
        const VkSurfaceKHR &getSurface() const { return surface; }
        const VkDevice &getDevice() const { return device; }
        const VkPhysicalDevice &getPhysicalDevice() const { return phyDevice; }
        const Queues &getQueues() const { return queues; }
        const RenderPass &getRenderPass() const { GET_NONNULL(renderPass, "No render pass"); }
        const SwapChain &getSwapChain() const { GET_NONNULL(swapChain, "No swap chain"); }
        const Pipeline &getPipeline() const { GET_NONNULL(pipeline, "No graphics pipeline"); }
        const CommandBuffer &getCommandBuffer() const { GET_NONNULL(cmdBuffer, "No command buffer"); }
        
    private:
        GLFWwindow *window;
        uint32_t width, height;
        VkInstance instance;
        VkSurfaceKHR surface;
        VkDevice device;
        VkPhysicalDevice phyDevice;
        Queues queues;
        RenderPass *renderPass;
        SwapChain *swapChain;
        Pipeline *pipeline;
        CommandBuffer *cmdBuffer;
#ifdef DEBUG
        VkDebugUtilsMessengerEXT callback;
#endif /* DEBUG */
        
        void initWindow();
        void initVulkan();
    };
    
#endif /* application_hpp */
}
