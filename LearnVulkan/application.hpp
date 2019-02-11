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

#include "basicobject.hpp"
#include "commandbuffer.hpp"
#include "pipeline.hpp"
#include "renderpass.hpp"
#include "swapchain.hpp"
#include "utils.hpp"
#include "validation.hpp"

class GLFWwindow;

namespace VulkanWrappers {
    using namespace std;
    
    class Application {
    public:
        Application(const string &vertFile,
                    const string &fragFile,
                    uint32_t width  = 800,
                    uint32_t height = 600);
        void mainLoop();
        void recreate();
        void cleanup();
        ~Application();
        
        bool frameBufferResized = false;
        VkExtent2D getCurrentExtent()               const;
        GLFWwindow *getWindow()                     const { return window; }
        const Instance &getInstance()               const { return instance; }
        const Surface &getSurface()                 const { return surface; }
        const PhysicalDevice &getPhysicalDevice()   const { return phyDevice; }
        const Device &getDevice()                   const { return device; }
        const SwapChain &getSwapChain()             const { return swapChain; }
        const RenderPass &getRenderPass()           const { return renderPass; }
        const Pipeline &getPipeline()               const { return pipeline; }
        const CommandBuffer &getCommandBuffer()     const { return cmdBuffer; }
        const Queues &getQueues()                   const { return queues; }
        Queues &getQueues()                               { return queues; }
        
    private:
        bool firstTime = true;
        GLFWwindow *window;
        Instance instance;
        Surface surface;
        Queues queues;
        PhysicalDevice phyDevice;
        Device device;
        SwapChain swapChain;
        RenderPass renderPass;
        Pipeline pipeline;
        CommandBuffer cmdBuffer;
#ifdef DEBUG
        DebugCallback callback;
#endif /* DEBUG */
        
        void initWindow(uint32_t width, uint32_t height);
        void initVulkan();
    };
    
#endif /* application_hpp */
}
