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

#include "util.h"

namespace vulkan {

using glm::vec2;
using glm::vec3;
using std::array;
using std::vector;
class Application;

struct VertexAttrib {
    vec2 pos;
    vec3 color;
    static array<VkVertexInputBindingDescription, 1> binding_descriptions();
    static array<VkVertexInputAttributeDescription, 2> attrib_descriptions();
};

extern const vector<VertexAttrib> kTriangleVertices;

class VertexBuffer { // allocate space on device; doesn't depend on swap chain
    const Application &app_;
    VkBuffer buffer_;
    VkDeviceMemory device_memory_;
    uint32_t vertex_count_;
    
public:
    VertexBuffer(const Application& app) : app_{app} {}
    void Init(const void* data, size_t data_size, size_t vertex_count);
    void Draw(const VkCommandBuffer& command_buffer) const;
    ~VertexBuffer();
    MARK_NOT_COPYABLE_OR_MOVABLE(VertexBuffer);
};

} /* namespace vulkan */

#endif /* LEARNVULKAN_VERTEX_BUFFER_H */
