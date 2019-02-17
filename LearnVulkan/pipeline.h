//
//  pipeline.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_PIPELINE_H
#define LEARNVULKAN_PIPELINE_H

#include <string>

#include <vulkan/vulkan.hpp>

namespace vulkan {
    using namespace std;
    
    class Application;
    
    class Pipeline {
        const Application &app;
        const string vertFile, fragFile;
        VkPipelineLayout layout;
        VkPipeline pipeline;
        
    public:
        Pipeline(const Application &app,
                 const string &vertFile,
                 const string &fragFile)
        : app{app}, vertFile{vertFile}, fragFile{fragFile} {}
        void init();
        void cleanup();
        ~Pipeline();
        
        const VkPipeline &operator*(void) const { return pipeline; }
    };
}

#endif /* LEARNVULKAN_PIPELINE_H */
