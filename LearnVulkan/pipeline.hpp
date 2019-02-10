//
//  pipeline.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef pipeline_hpp
#define pipeline_hpp

#include <vulkan/vulkan.hpp>

#include "renderpass.hpp"

namespace VulkanWrappers {
    using namespace std;
    
    class Pipeline {
        const VkDevice &device;
        VkPipelineLayout layout;
        VkPipeline pipeline;
    public:
        Pipeline(const VkDevice &device,
                 const VkRenderPass &renderPass,
                 VkExtent2D imageExtent);
        const VkPipeline &getVkPipeline() const { return pipeline; }
        ~Pipeline();
    };
}

#endif /* pipeline_hpp */
