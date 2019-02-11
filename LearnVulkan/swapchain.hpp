//
//  swapchain.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef swapchain_hpp
#define swapchain_hpp

#include <vector>

#include <vulkan/vulkan.hpp>

namespace VulkanWrappers {
    using namespace std;
    
    class Application;
    class Surface;
    class PhysicalDevice;
    
    class SwapChain {
        const Application &app;
        VkSwapchainKHR swapChain;
        vector<VkImage> images;
        vector<VkImageView> imageViews;
        VkFormat imageFormat;
        VkExtent2D imageExtent;
        
    public:
        static const vector<const char*> requiredExtensions;
        static bool hasSwapChainSupport(const Surface &surface,
                                        const PhysicalDevice &phyDevice);
        
        SwapChain(const Application &app) : app{app} {}
        void init();
        void cleanup();
        ~SwapChain();
        
        const VkSwapchainKHR &operator*(void)       const { return swapChain; }
        const vector<VkImageView> &getImageViews()  const { return imageViews; }
        VkFormat getFormat()                        const { return imageFormat; }
        VkExtent2D getExtent()                      const { return imageExtent; }
    };
}

#endif /* swapchain_hpp */
