//
//  triangle_app.h
//  LearnVulkan
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef TRIANGLE_APP_H
#define TRIANGLE_APP_H

#include <array>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace vulkan {

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/
//    chap14.html#interfaces-resources-layout
struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

struct VertexAttrib {
  glm::vec2 pos;
  glm::vec3 color;
  static std::array<VkVertexInputBindingDescription, 1> binding_descriptions();
  static std::array<VkVertexInputAttributeDescription, 2> attrib_descriptions();
  static const void* ubo();
  static size_t ubo_size() { return sizeof(glm::mat4) * 3; }
  static void UpdateUbo(size_t current_frame, float screen_aspect);
};

extern const std::vector<VertexAttrib> kTriangleVertices;
extern const std::vector<uint32_t> kTrangleIndices;

} /* namespace vulkan */

#endif /* TRIANGLE_APP_H */
