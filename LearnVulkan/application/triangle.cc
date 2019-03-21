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

#include "util.h"

namespace application {
namespace vulkan {
namespace {

using std::vector;
using util::VertexAttrib;
using wrapper::vulkan::buffer::DataInfo;
using wrapper::vulkan::buffer::ChunkInfo;
using wrapper::vulkan::descriptor::ResourceInfo;
using wrapper::vulkan::Descriptor;

size_t kNumFrameInFlight{2};

vector<VkVertexInputBindingDescription> BindingDescriptions() {
  vector<VkVertexInputBindingDescription> binding_descs(1);

  binding_descs[0] = VkVertexInputBindingDescription{
      /*binding=*/0,
      /*stride=*/sizeof(VertexAttrib),
      // for instancing, use _INSTANCE for .inputRate
      /*inputRate=*/VK_VERTEX_INPUT_RATE_VERTEX,
  };

  return binding_descs;
}

vector<VkVertexInputAttributeDescription> AttribDescriptions() {
  vector<VkVertexInputAttributeDescription> attrib_descs(3);

  attrib_descs[0] = VkVertexInputAttributeDescription{
      /*location=*/0, // layout (location = 0) in
      /*binding=*/0, // which binding point does data come from
      /*format=*/VK_FORMAT_R32G32B32_SFLOAT, // implies total size
      /*offset=*/offsetof(VertexAttrib, pos), // reading offset
  };

  attrib_descs[1] = VkVertexInputAttributeDescription{
      /*location=*/1,
      /*binding=*/0,
      /*format=*/VK_FORMAT_R32G32B32_SFLOAT,
      /*offset=*/offsetof(VertexAttrib, norm),
  };

  attrib_descs[2] = VkVertexInputAttributeDescription{
      /*location=*/2,
      /*binding=*/0,
      /*format=*/VK_FORMAT_R32G32_SFLOAT,
      /*offset=*/offsetof(VertexAttrib, tex_coord),
  };

  return attrib_descs;
}

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/\
//    chap14.html#interfaces-resources-layout
struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

vector<UniformBufferObject> kUbo;

void UpdateUbo(size_t current_frame, float screen_aspect) {
  static auto start_time = std::chrono::high_resolution_clock::now();
  auto current_time = std::chrono::high_resolution_clock::now();
  auto time = std::chrono::duration<float, std::chrono::seconds::period>(
      current_time - start_time).count();
  UniformBufferObject& ubo = kUbo[current_frame];
  ubo.model = glm::rotate(glm::mat4{1.0f}, time * glm::radians(90.0f),
                          glm::vec3{0.0f, 0.0f, 1.0f});
  ubo.view = glm::lookAt(glm::vec3{2.0f}, glm::vec3{0.0f},
                         glm::vec3{0.0f, 0.0f, 1.0f});
  ubo.proj = glm::perspective(glm::radians(45.0f), screen_aspect, 0.1f, 10.0f);
  // No need to flip Y-axis as OpenGL
  ubo.proj[1][1] *= -1;
}

} /* namespace */

void TriangleApplication::Init() {
  if (is_first_time) {
    vector<VertexAttrib> vertices;
    vector<uint32_t> indices;
    util::LoadObjFile("texture/square.obj", 1, vertices, indices);

    // vertex buffer
    DataInfo vertex_info{
        vertices.data(),
        sizeof(vertices[0]) * vertices.size(),
        CONTAINER_SIZE(vertices),
    };
    DataInfo index_info{
        indices.data(),
        sizeof(indices[0]) * indices.size(),
        CONTAINER_SIZE(indices),
    };
    vertex_buffer_.Init(context_->ptr(), vertex_info, index_info);

    // uniform buffer
    kUbo.resize(context_->swapchain().size());
    ChunkInfo chunk_info{
        kUbo.data(),
        sizeof(UniformBufferObject),
        CONTAINER_SIZE(kUbo),
    };
    uniform_buffer_.Init(context_->ptr(), chunk_info);

    // texture
    image_.Init(context_, "texture/statue.jpg");

    // descriptor
    resource_infos_ = {
        ResourceInfo{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {0},
                     VK_SHADER_STAGE_VERTEX_BIT},
        ResourceInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {1},
                     VK_SHADER_STAGE_FRAGMENT_BIT},
    };
    descriptors_.reserve(kNumFrameInFlight);
    for (size_t i = 0; i < kNumFrameInFlight; ++i) {
      descriptors_.emplace_back(std::make_unique<Descriptor>());
      descriptors_[i]->Init(context_, resource_infos_);
      descriptors_[i]->UpdateBufferInfos(resource_infos_[0],
                                         {uniform_buffer_.descriptor_info(i)});
      descriptors_[i]->UpdateImageInfos(resource_infos_[1],
                                        {image_.descriptor_info()});
    }

    is_first_time = false;
  }

  pipeline_.Init(context_->ptr(), "compiled/triangle.vert.spv",
                 "compiled/triangle.frag.spv", descriptors_[0]->layout(),
                 BindingDescriptions(), AttribDescriptions());
  command_.Init(context_->ptr(), kNumFrameInFlight,
                [&](const VkCommandBuffer& command_buffer, size_t image_index) {
    // start render pass
    VkClearValue clear_color{0.0f, 0.0f, 0.0f, 1.0f};
    VkRenderPassBeginInfo begin_info{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        /*pNext=*/nullptr,
        *context_->render_pass(),
        context_->render_pass().framebuffers()[image_index],
        /*renderArea=*/{
            /*offset=*/{0, 0},
            context_->swapchain().extent(),
        },
        /*clearValueCount=*/1,
        &clear_color,  // used for _OP_CLEAR
    };

    // record commends. options:
    //   - VK_SUBPASS_CONTENTS_INLINE: use primary commmand buffer
    //   - VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: use secondary
    vkCmdBeginRenderPass(command_buffer, &begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      *pipeline_);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.layout(), 0, 1,
                            &descriptors_[image_index]->set(), 0, nullptr);
    vertex_buffer_.Draw(command_buffer);

    vkCmdEndRenderPass(command_buffer);
  });
}

void TriangleApplication::Cleanup() {
  command_.Cleanup();
  pipeline_.Cleanup();
}

void TriangleApplication::MainLoop() {
  Init();
  while (!context_->ShouldQuit()) {
    const VkExtent2D extent = context_->swapchain().extent();
    auto update_func = [this, extent](size_t image_index) {
      UpdateUbo(image_index, (float)extent.width / extent.height);
      uniform_buffer_.Update(image_index);
    };
    if (command_.DrawFrame(current_frame_, update_func) != VK_SUCCESS ||
        context_->resized()) {
      context_->resized() = false;
      context_->WaitIdle();
      Cleanup();
      context_->Recreate();
      Init();
    }
    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
  }
  context_->WaitIdle(); // wait for all async operations finish
}

} /* namespace vulkan */
} /* namespace application */
