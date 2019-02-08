//
//  renderpass.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef renderpass_hpp
#define renderpass_hpp

#include <vulkan/vulkan.hpp>

namespace VulkanWrappers {
    using namespace std;
    
    class RenderPass {
        const VkDevice &device;
        VkRenderPass renderPass;
    public:
        RenderPass(const VkDevice &device, VkFormat colorAttFormat);
        const VkRenderPass &handle() const { return renderPass; }
        ~RenderPass();
    };
}

#endif /* renderpass_hpp */
