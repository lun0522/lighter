//
//  cube.cc
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <iostream>

#include "absl/types/optional.h"
#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/common/time.h"
#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/context.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
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

using namespace wrapper::vulkan;

constexpr int kNumFrameInFlight = 2;

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct Transformation {
  alignas(16) glm::mat4 proj_view_model;
};

class CubeApp {
 public:
  CubeApp() : context_{Context::GetContext()} {
    context_->Init("Cube");
  };
  void MainLoop();

 private:
  void Init();
  void UpdateData(int frame, float screen_aspect);
  void Cleanup();

  bool is_first_time = true;
  int current_frame_ = 0;
  common::Timer timer_;
  std::shared_ptr<Context> context_;
  Command command_;
  Model model_;
  PushConstant push_constant_;
};

} /* namespace */

void CubeApp::Init() {
  if (is_first_time) {
    // push constants
    push_constant_.Init(sizeof(Transformation), kNumFrameInFlight);

    is_first_time = false;
  }

  // model
  Model::TextureBindingMap bindings{};
  bindings[Model::TextureType::kTypeDiffuse] = {
      /*binding_point=*/1,
      {{"external/resource/texture/statue.jpg"}},
  };
  model_.Init(context_,
              {{VK_SHADER_STAGE_VERTEX_BIT,
                "jessie_steamer/shader/vulkan/simple.vert.spv"},
               {VK_SHADER_STAGE_FRAGMENT_BIT,
                "jessie_steamer/shader/vulkan/simple.frag.spv"}},
              Model::SingleMeshResource{
                  "external/resource/model/cube.obj",
                  /*obj_index_base=*/1, bindings},
              /*uniform_infos=*/absl::nullopt,
              /*instancing_info=*/absl::nullopt,
              absl::make_optional<Model::PushConstantInfos>(
                  {{VK_SHADER_STAGE_VERTEX_BIT,
                    {{&push_constant_, /*offset=*/0}}}}),
              kNumFrameInFlight, /*is_opaque=*/true);

  // command buffer
  command_.Init(context_, kNumFrameInFlight);
}

void CubeApp::UpdateData(int frame, float screen_aspect) {
  const float elapsed_time = timer_.time_from_launch();
  glm::mat4 model = glm::rotate(glm::mat4{1.0f},
                                elapsed_time * glm::radians(90.0f),
                                glm::vec3{1.0f, 1.0f, 0.0f});
  glm::mat4 view = glm::lookAt(glm::vec3{3.0f}, glm::vec3{0.0f},
                               glm::vec3{0.0f, 0.0f, 1.0f});
  glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                    screen_aspect, 0.1f, 100.0f);
  *push_constant_.data<Transformation>(frame) = {proj * view * model};
}

void CubeApp::MainLoop() {
  Init();
  auto& window = context_->window();

  while (!window.ShouldQuit()) {
    const auto update_data = [this](int frame) {
      const VkExtent2D extent = context_->swapchain().extent();
      UpdateData(frame, (float)extent.width / extent.height);
    };
    const auto draw_result = command_.Draw(
        current_frame_, update_data,
        [&](const VkCommandBuffer& command_buffer,
            const VkFramebuffer& framebuffer) {
      // start render pass
      std::array<VkClearValue, 2> clear_values{};
      clear_values[0].color.float32[0] = 0.0f;
      clear_values[0].color.float32[1] = 0.0f;
      clear_values[0].color.float32[2] = 0.0f;
      clear_values[0].color.float32[3] = 1.0f;
      clear_values[1].depthStencil = {1.0f, 0};  // initial depth set to 1.0

      VkRenderPassBeginInfo begin_info{
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          /*pNext=*/nullptr,
          *context_->render_pass(),
          framebuffer,
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

      model_.Draw(command_buffer, current_frame_, /*instance_count=*/1);

      vkCmdEndRenderPass(command_buffer);
    });

    if (draw_result != VK_SUCCESS || window.IsResized()) {
      window.ResetResizedFlag();
      context_->WaitIdle();
      Cleanup();
      context_->Recreate();
      Init();
    }

    const auto frame_rate = timer_.frame_rate();
    if (frame_rate.has_value()) {
      std::cout << "Frame per second: " << frame_rate.value() << std::endl;
    }
    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
    window.PollEvents();
  }
  context_->WaitIdle();  // wait for all async operations finish
}

void CubeApp::Cleanup() {
  command_.Cleanup();
}

} /* namespace cube */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, const char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  SetBuildEnvironment();
#ifdef NDEBUG
  try {
    cube::CubeApp app{};
    app.MainLoop();
  } catch (const std::exception& e) {
    std::cerr << "Error: /n/t" << e.what() << std::endl;
    return EXIT_FAILURE;
  }
#else  /* !NDEBUG */
  cube::CubeApp app{};
  app.MainLoop();
#endif /* NDEBUG */
  return EXIT_SUCCESS;
}
