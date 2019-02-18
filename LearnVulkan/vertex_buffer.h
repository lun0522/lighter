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
    
    static array<VkVertexInputBindingDescription, 1> binding_descriptions() {
        array<VkVertexInputBindingDescription, 1> binding_descs{};
        
        binding_descs[0].binding = 0;
        binding_descs[0].stride = sizeof(Vertex);
        // for instancing, use _INSTANCE for .inputRate
        binding_descs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        return binding_descs;
    }
    
    static array<VkVertexInputAttributeDescription, 2> attrib_descriptions() {
        array<VkVertexInputAttributeDescription, 2> attrib_descs{};
        
        attrib_descs[0].binding = 0; // which binding point does data come from
        attrib_descs[0].location = 0; // layout (location = 0) in
        attrib_descs[0].format = VK_FORMAT_R32G32_SFLOAT; // implies total size
        attrib_descs[0].offset = offsetof(Vertex, pos); // start reading offset
        
        attrib_descs[1].binding = 0;
        attrib_descs[1].location = 1;
        attrib_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrib_descs[1].offset = offsetof(Vertex, color);
        
        return attrib_descs;
    }
};

static const vector<Vertex> vertices {
    {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
};

} /* namespace vulkan */

#endif /* LEARNVULKAN_VERTEX_BUFFER_H */
