//
//  commandbuffer.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef commandbuffer_hpp
#define commandbuffer_hpp

#include <vector>

#include <vulkan/vulkan.hpp>

namespace VulkanWrappers {
    using std::vector;
    
    class Application;
    
    class CommandBuffer {
        const Application &app;
        const size_t MAX_FRAMES_IN_FLIGHT = 2;
        size_t currentFrame = 0;
        vector<VkSemaphore> imageAvailableSemas;
        vector<VkSemaphore> renderFinishedSemas;
        vector<VkFence> inFlightFences;
        VkCommandPool commandPool;
        vector<VkCommandBuffer> commandBuffers;
        
        void createCommandObjects();
        void recordCommandBuffers();
        void createSyncObjects();
        
    public:
        CommandBuffer(const Application &app);
        void drawFrame();
        ~CommandBuffer();
    };
}

#endif /* commandbuffer_hpp */
