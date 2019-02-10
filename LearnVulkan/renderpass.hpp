//
//  renderpass.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef renderpass_hpp
#define renderpass_hpp

#include <vector>

#include <vulkan/vulkan.hpp>

namespace VulkanWrappers {
    using namespace std;
    
    class RenderPass {
        const VkDevice &device;
        VkRenderPass renderPass;
        vector<VkFramebuffer> framebuffers;
    public:
        RenderPass(const VkDevice &device,
                   VkFormat colorAttFormat,
                   VkExtent2D imageExtent,
                   const vector<VkImageView> &imageViews);
        const VkRenderPass &getVkRenderPass() const { return renderPass; }
        const vector<VkFramebuffer> &getFramebuffers() const { return framebuffers; }
        ~RenderPass();
    };
}

#endif /* renderpass_hpp */
