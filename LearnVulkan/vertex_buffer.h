//
//  vertex_buffer.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_VERTEX_BUFFER_H
#define LEARNVULKAN_VERTEX_BUFFER_H

#include <array>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace vulkan {
    using namespace glm;
    using namespace std;
    
    struct Vertex {
        vec2 pos;
        vec3 color;
        
        static array<VkVertexInputBindingDescription, 1> getBindingDescriptions() {
            array<VkVertexInputBindingDescription, 1> bindingDescs{};
            
            bindingDescs[0].binding = 0;
            bindingDescs[0].stride = sizeof(Vertex);
            bindingDescs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // for instancing, use _INSTANCE instead
            
            return bindingDescs;
        }
        
        static array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
            array<VkVertexInputAttributeDescription, 2> attributeDescs{};
            
            attributeDescs[0].binding = 0; // which binding point does the data come from
            attributeDescs[0].location = 0; // layout (location = 0) in
            attributeDescs[0].format = VK_FORMAT_R32G32_SFLOAT; // implies total size
            attributeDescs[0].offset = offsetof(Vertex, pos); // start reading offset
            
            attributeDescs[1].binding = 0;
            attributeDescs[1].location = 1;
            attributeDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescs[1].offset = offsetof(Vertex, color);
            
            return attributeDescs;
        }
    };
    
    static const vector<Vertex> vertices {
        {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    };
}

#endif /* LEARNVULKAN_VERTEX_BUFFER_H */
