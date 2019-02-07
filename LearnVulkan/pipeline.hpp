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

namespace VulkanWrappers {
    using namespace std;
    
    class Pipeline {
        const VkDevice &device;
        VkPipelineLayout layout;
    public:
        Pipeline(const VkDevice &device, VkExtent2D currentExtent);
        ~Pipeline();
    };
}

#endif /* pipeline_hpp */
