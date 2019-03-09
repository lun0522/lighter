//
//  triangle.cc
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "triangle.h"

#include <chrono>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

using std::vector;

namespace vulkan {
namespace application {
namespace {

const vector<VertexAttrib> kTriangleVertices {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
};

const vector<uint32_t> kTrangleIndices {
    0, 1, 2, 2, 3, 0,
};

} /* namespace */

vector<VkVertexInputBindingDescription> VertexAttrib::binding_descriptions() {
  vector<VkVertexInputBindingDescription> binding_descs(1);

  binding_descs[0].binding = 0;
  binding_descs[0].stride = sizeof(VertexAttrib);
  // for instancing, use _INSTANCE for .inputRate
  binding_descs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return binding_descs;
}

vector<VkVertexInputAttributeDescription> VertexAttrib::attrib_descriptions() {
  vector<VkVertexInputAttributeDescription> attrib_descs(2);

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

static std::array<UniformBufferObject, wrapper::Command::kMaxFrameInFlight>
    kUbo{};

const void* VertexAttrib::ubo() {
  return kUbo.data();
}

void VertexAttrib::UpdateUbo(size_t current_frame, float screen_aspect) {
  static auto start_time = std::chrono::high_resolution_clock::now();
  auto current_time = std::chrono::high_resolution_clock::now();
  auto time = std::chrono::duration<float, std::chrono::seconds::period>(
      current_time - start_time).count();
  UniformBufferObject& ubo = kUbo[current_frame];
  ubo.model = glm::rotate(glm::mat4{1.0f}, time * glm::radians(90.0f),
                          {0.0f, 0.0f, 1.0f});
  ubo.view = glm::lookAt(glm::vec3{2.0f}, glm::vec3{0.0f}, {0.0f, 0.0f, 1.0f});
  ubo.proj = glm::perspective(glm::radians(45.0f), screen_aspect, 0.1f, 10.0f);
  // No need to flip Y-axis as OpenGL
  ubo.proj[1][1] *= -1;
}

void TriangleApplication::Init() {
  if (is_first_time) {
    vertex_buffer_.Init(context_->ptr(),
                        kTriangleVertices.data(),
                        sizeof(kTriangleVertices[0]) * kTriangleVertices.size(),
                        kTriangleVertices.size(),
                        kTrangleIndices.data(),
                        sizeof(kTrangleIndices[0]) * kTrangleIndices.size(),
                        kTrangleIndices.size());
    uniform_buffer_.Init(context_->ptr(),
                         VertexAttrib::ubo(),
                         wrapper::Command::kMaxFrameInFlight,
                         VertexAttrib::ubo_size());
    is_first_time = false;
  }

  pipeline_.Init(context_->ptr(), "triangle.vert.spv", "triangle.frag.spv",
                 uniform_buffer_, VertexAttrib::binding_descriptions(),
                 VertexAttrib::attrib_descriptions());
  command_.Init(context_->ptr(), pipeline_, vertex_buffer_, uniform_buffer_);
}

void TriangleApplication::Cleanup() {
  command_.Cleanup();
  pipeline_.Cleanup();
}

void TriangleApplication::MainLoop() {
  Init();
  while (!context_->ShouldQuit()) {
    const VkExtent2D extent = context_->swapchain().extent();
    size_t current_frame = command_.current_frame();
    auto update_func = [=](size_t current_frame_) {
      VertexAttrib::UpdateUbo(
          current_frame, (float)extent.width / extent.height);
    };
    if (command_.DrawFrame(uniform_buffer_, update_func) != VK_SUCCESS ||
        context_->resized()) {
      context_->resized() = false;
      context_->WaitIdle();
      Cleanup();
      context_->Recreate();
      Init();
    }
  }
  context_->WaitIdle(); // wait for all async operations finish
}

} /* namespace application */
} /* namespace vulkan */
