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
    
    class SwapChain {
        const Application &app;
        VkSwapchainKHR swapChain;
        vector<VkImage> images;
        vector<VkImageView> imageViews;
        VkFormat imageFormat;
        VkExtent2D imageExtent;
        void createImages();
    public:
        static const vector<const char*> requiredExtensions;
        static bool hasSwapChainSupport(const VkSurfaceKHR &surface,
                                        const VkPhysicalDevice &device);
        SwapChain(const Application &app);
        const VkSwapchainKHR &operator*(void) const { return swapChain; }
        const vector<VkImageView> &getImageViews() const { return imageViews; }
        VkFormat getFormat() const { return imageFormat; }
        VkExtent2D getExtent() const { return imageExtent; }
        ~SwapChain();
    };
}

#endif /* swapchain_hpp */
