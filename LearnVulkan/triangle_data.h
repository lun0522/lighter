//
//  triangle_data.h
//  LearnVulkan
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef TRIANGLE_DATA_H
#define TRIANGLE_DATA_H

#include <array>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace vulkan {

struct VertexAttrib {
  glm::vec2 pos;
  glm::vec3 color;
  static std::array<VkVertexInputBindingDescription, 1> binding_descriptions();
  static std::array<VkVertexInputAttributeDescription, 2> attrib_descriptions();
};

extern const std::vector<VertexAttrib> kTriangleVertices;
extern const std::vector<uint32_t> kTrangleIndices;

} /* namespace vulkan */

#endif /* TRIANGLE_DATA_H */
