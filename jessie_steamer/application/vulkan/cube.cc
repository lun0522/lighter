//
//  cube.cc
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "third_party/glm/glm.hpp"
// different from OpenGL, where depth values are in range [-1.0, 1.0]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "third_party/glm/gtc/matrix_transform.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace cube {
namespace {

namespace util = common::util;
using namespace wrapper::vulkan;

constexpr size_t kNumFrameInFlight = 2;

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct Transformation {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

class CubeApp {
 public:
  CubeApp() : context_{Context::CreateContext()} {
    context_->Init("Cube");
  };
  void MainLoop();

 private:
  void Init();
  void UpdateTrans(size_t frame_index,
                   float screen_aspect);
  void Cleanup();

  bool is_first_time = true;
  size_t current_frame_ = 0;
  std::shared_ptr<Context> context_;
  Command command_;
  Model model_;
  UniformBuffer uniform_buffer_;
};

} /* namespace */

void CubeApp::Init() {
  if (is_first_time) {
    // uniform buffer
    uniform_buffer_.Init(context_, UniformBuffer::Info{
        sizeof(Transformation),
        context_->swapchain().size(),
    });

    is_first_time = false;
  }

  // model
  Model::TextureBindingMap bindings;
  bindings[Model::TextureType::kTypeDiffuse] = {
      /*binding_point=*/1,
      {{"jessie_steamer/resource/texture/statue.jpg"}},
  };
  Descriptor::Info uniform_desc_info{
    /*descriptor_type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    /*shader_stage=*/VK_SHADER_STAGE_VERTEX_BIT,
    /*bindings=*/{{
        Descriptor::TextureType::kTypeMaxEnum,
        /*binding_point=*/0,
        /*array_length=*/1,
    }},
  };
  model_.Init(context_,
              {{"jessie_steamer/shader/compiled/simple.vert.spv",
                VK_SHADER_STAGE_VERTEX_BIT},
               {"jessie_steamer/shader/compiled/simple.frag.spv",
                VK_SHADER_STAGE_FRAGMENT_BIT}},
              {{&uniform_buffer_, &uniform_desc_info}},
              Model::SingleMeshResource{
                  "jessie_steamer/resource/model/cube.obj",
                  /*obj_index_base=*/1, bindings},
              kNumFrameInFlight);

  // command
  command_.Init(context_, kNumFrameInFlight,
                [&](const VkCommandBuffer& command_buffer, size_t image_index) {
    // start render pass
    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color.float32[0] = 0.0f;
    clear_values[0].color.float32[1] = 0.0f;
    clear_values[0].color.float32[2] = 0.0f;
    clear_values[0].color.float32[3] = 1.0f;
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
    //   - VK_SUBPASS_CONTENTS_INLINE: use primary command buffer
    //   - VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: use secondary
    vkCmdBeginRenderPass(command_buffer, &begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    model_.Draw(command_buffer, image_index);

    vkCmdEndRenderPass(command_buffer);
  });
}

void CubeApp::UpdateTrans(size_t frame_index,
                          float screen_aspect) {
  static auto start_time = util::Now();
  auto elapsed_time = util::TimeInterval(start_time, util::Now());
  auto* trans = uniform_buffer_.data<Transformation>(frame_index);
  *trans = {
      glm::rotate(glm::mat4{1.0f}, elapsed_time * glm::radians(90.0f),
                  glm::vec3{1.0f, 1.0f, 0.0f}),
      glm::lookAt(glm::vec3{3.0f}, glm::vec3{0.0f},
                  glm::vec3{0.0f, 0.0f, 1.0f}),
      glm::perspective(glm::radians(45.0f), screen_aspect, 0.1f, 100.0f),
  };
  // No need to flip Y-axis as OpenGL
  trans->proj[1][1] *= -1;
}

void CubeApp::MainLoop() {
  Init();
  auto& window = context_->window();
  while (!window.ShouldQuit()) {
    window.PollEvents();
    VkExtent2D extent = context_->swapchain().extent();
    auto update_func = [this, extent](size_t frame_index) {
      UpdateTrans(frame_index, (float)extent.width / extent.height);
      uniform_buffer_.UpdateData(frame_index);
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
  context_->WaitIdle();  // wait for all async operations finish
}

void CubeApp::Cleanup() {
  command_.Cleanup();
  model_.Cleanup();
}

} /* namespace cube */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, const char* argv[]) {
#ifdef DEBUG
  INSERT_DEBUG_REQUIREMENT(/*overwrite=*/true);
  jessie_steamer::application::vulkan::cube::CubeApp app{};
  app.MainLoop();
#else
  try {
    jessie_steamer::application::vulkan::cube::CubeApp app{};
    app.MainLoop();
  } catch (const std::exception& e) {
    std::cerr << "Error: /n/t" << e.what() << std::endl;
    return EXIT_FAILURE;
  }
#endif /* DEBUG */
  return EXIT_SUCCESS;
}
