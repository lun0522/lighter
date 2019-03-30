//
//  cube.cc
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "cube.h"

#include <array>
#include <chrono>
#include <iostream>

#define GLM_FORCE_RADIANS
// different from OpenGL, where depth values are in range [-1.0, 1.0]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include "util.h"

namespace application {
namespace vulkan {
namespace {

using std::vector;

constexpr size_t kNumFrameInFlight = 2;

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct Transformation {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

vector<Transformation> kTrans;

void UpdateTrans(size_t current_frame, float screen_aspect) {
  static auto start_time = util::Now();
  auto elapsed_time = util::TimeInterval(start_time, util::Now());
  Transformation& trans = kTrans[current_frame];
  trans = {
    glm::rotate(glm::mat4{1.0f}, elapsed_time * glm::radians(90.0f),
                glm::vec3{1.0f, 1.0f, 0.0f}),
    glm::lookAt(glm::vec3{3.0f}, glm::vec3{0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}),
    glm::perspective(glm::radians(45.0f), screen_aspect, 0.1f, 100.0f),
  };
  // No need to flip Y-axis as OpenGL
  trans.proj[1][1] *= -1;
}

} /* namespace */

void CubeApp::Init() {
  if (is_first_time) {
    // model (vertex buffer)
    model_.Init(context_->ptr(), "texture/cube.obj", 1);

    // uniform buffer
    kTrans.resize(context_->swapchain().size());
    buffer::ChunkInfo chunk_info{
        kTrans.data(),
        sizeof(Transformation),
        CONTAINER_SIZE(kTrans),
    };
    uniform_buffer_.Init(context_->ptr(), chunk_info);

    // texture
    image_.Init(context_, {"texture/statue.jpg"});

    // descriptor
    resource_infos_ = {
        descriptor::ResourceInfo{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {0},
                                 VK_SHADER_STAGE_VERTEX_BIT},
        descriptor::ResourceInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {1},
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

  depth_stencil_.Init(context_, context_->swapchain().extent());
  context_->render_pass().Config(depth_stencil_);
  pipeline_.Init(context_->ptr(),
                 {{"compiled/simple.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
                  {"compiled/simple.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}},
                 descriptors_[0]->layout(),
                 model_.binding_descs(), model_.attrib_descs());
  command_.Init(context_->ptr(), kNumFrameInFlight,
                [&](const VkCommandBuffer& command_buffer, size_t image_index) {
    // start render pass
    std::array<VkClearValue, 2> clear_values;
    clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clear_values[1].depthStencil = {1.0f, 0};  // initial depth value set to 1.0

    VkRenderPassBeginInfo begin_info{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        /*pNext=*/nullptr,
        *context_->render_pass(),
        context_->render_pass().framebuffer(image_index),
        /*renderArea=*/{
            /*offset=*/{0, 0},
            context_->swapchain().extent(),
        },
        CONTAINER_SIZE(clear_values),
        clear_values.data(),  // used for _OP_CLEAR
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
    model_.Draw(command_buffer);

    vkCmdEndRenderPass(command_buffer);
  });
}

void CubeApp::Cleanup() {
  command_.Cleanup();
  pipeline_.Cleanup();
}

void CubeApp::MainLoop() {
  Init();
  auto& window = context_->window();
  while (!window.ShouldQuit()) {
    window.PollEvents();
    VkExtent2D extent = context_->swapchain().extent();
    auto update_func = [this, extent](size_t image_index) {
      UpdateTrans(image_index, (float)extent.width / extent.height);
      uniform_buffer_.Update(image_index);
    };
    if (command_.DrawFrame(current_frame_, update_func) != VK_SUCCESS ||
        window.IsResized()) {
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

int main(int argc, const char* argv[]) {
#ifdef DEBUG
  application::vulkan::NanosuitApp app{};
  app.MainLoop();
#else
  try {
    application::vulkan::NanosuitApp app{};
    app.MainLoop();
  } catch (const std::exception& e) {
    std::cerr << "Error: /n/t" << e.what() << std::endl;
    return EXIT_FAILURE;
  }
#endif /* DEBUG */
  return EXIT_SUCCESS;
}
