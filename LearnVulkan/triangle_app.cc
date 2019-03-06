//
//  triangle_data.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "triangle_app.h"

#include <chrono>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;
using namespace std;

namespace vulkan {

const vector<VertexAttrib> kTriangleVertices {
  {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
  {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
  {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
  {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
};

const vector<uint32_t> kTrangleIndices {
  0, 1, 2, 2, 3, 0,
};

array<VkVertexInputBindingDescription, 1> VertexAttrib::binding_descriptions() {
  array<VkVertexInputBindingDescription, 1> binding_descs{};

  binding_descs[0].binding = 0;
  binding_descs[0].stride = sizeof(VertexAttrib);
  // for instancing, use _INSTANCE for .inputRate
  binding_descs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return binding_descs;
}

array<VkVertexInputAttributeDescription, 2> VertexAttrib::attrib_descriptions() {
  array<VkVertexInputAttributeDescription, 2> attrib_descs{};

  attrib_descs[0].binding = 0; // which binding point does data come from
  attrib_descs[0].location = 0; // layout (location = 0) in
  attrib_descs[0].format = VK_FORMAT_R32G32_SFLOAT; // implies total size
  attrib_descs[0].offset = offsetof(VertexAttrib, pos); // reading offset

  attrib_descs[1].binding = 0;
  attrib_descs[1].location = 1;
  attrib_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attrib_descs[1].offset = offsetof(VertexAttrib, color);

  return attrib_descs;
}

static std::array<UniformBufferObject, 2> kUbo{};  //  TODO: ~2

const void* VertexAttrib::ubo() {
  return kUbo.data();
}

void VertexAttrib::UpdateUbo(size_t current_frame, float screen_aspect) {
  static auto start_time = chrono::high_resolution_clock::now();
  auto current_time = chrono::high_resolution_clock::now();
  auto time = chrono::duration<float, chrono::seconds::period>(
      current_time - start_time).count();
  UniformBufferObject& ubo = kUbo[current_frame];
  ubo.model = rotate(mat4{1.0f}, time * radians(90.0f), {0.0f, 0.0f, 1.0f});
  ubo.view = lookAt(vec3{2.0f}, vec3{0.0f}, {0.0f, 0.0f, 1.0f});
  ubo.proj = perspective(radians(45.0f), screen_aspect, 0.1f, 10.0f);
  // No need to flip Y-axis as OpenGL
  ubo.proj[1][1] *= -1;
}

} /* namespace vulkan */
