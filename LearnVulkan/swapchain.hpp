//
//  swapchain.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef swapchain_hpp
#define swapchain_hpp

#include <string>
#include <unordered_set>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace VulkanWrappers {
    using namespace std;
    
    class SwapChain {
        const VkDevice &device;
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
        SwapChain(const VkSurfaceKHR &surface,
                  const VkDevice &device,
                  const VkPhysicalDevice &physicalDevice,
                  VkExtent2D currentExtent,
                  const unordered_set<uint32_t> &queueFamilies);
        ~SwapChain();
    };
}

#endif /* swapchain_hpp */
