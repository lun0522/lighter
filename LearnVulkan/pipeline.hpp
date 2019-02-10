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
    using std::vector;
    
    class Application;
    
    class Pipeline {
        const Application &app;
        VkPipelineLayout layout;
        VkPipeline pipeline;
    public:
        Pipeline(const Application &app);
        const VkPipeline &operator*(void) const { return pipeline; }
        ~Pipeline();
    };
}

#endif /* pipeline_hpp */
