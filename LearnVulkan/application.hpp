//
//  application.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef application_hpp
#define application_hpp

#include <string>

#include <vulkan/vulkan.hpp>

#include "commandbuffer.hpp"
#include "pipeline.hpp"
#include "renderpass.hpp"
#include "swapchain.hpp"
#include "utils.hpp"
#include "vkobjects.hpp"

class GLFWwindow;

namespace VulkanWrappers {
    using namespace std;
    
    class Application {
    public:
        struct Queues {
            VkQueue graphicsQueue;
            VkQueue presentQueue;
            uint32_t graphicsFamily;
            uint32_t presentFamily;
        };
        
        Application(const string &vertFile,
                    const string &fragFile,
                    uint32_t width  = 800,
                    uint32_t height = 600);
        void mainLoop();
        void recreate();
        void initAll();
        void cleanupAll();
        ~Application();
        
        bool frameBufferResized = false;
        VkExtent2D getCurrentExtent()               const;
        const Surface &getSurface()                 const { return surface; }
        const Device &getDevice()                   const { return device; }
        const PhysicalDevice &getPhysicalDevice()   const { return phyDevice; }
        const Queues &getQueues()                   const { return queues; }
        const SwapChain &getSwapChain()             const { return swapChain; }
        const RenderPass &getRenderPass()           const { return renderPass; }
        const Pipeline &getPipeline()               const { return pipeline; }
        const CommandBuffer &getCommandBuffer()     const { return cmdBuffer; }
        
    private:
        GLFWwindow *window;
        Instance instance;
        Surface surface;
        Device device;
        PhysicalDevice phyDevice;
        Queues queues;
        SwapChain swapChain;
        RenderPass renderPass;
        Pipeline pipeline;
        CommandBuffer cmdBuffer;
#ifdef DEBUG
        VkDebugUtilsMessengerEXT callback;
#endif /* DEBUG */
        
        void initWindow(uint32_t width, uint32_t height);
        void initVulkan();
    };
    
#endif /* application_hpp */
}
