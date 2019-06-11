//
//  cube.cc
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <iostream>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/common/time.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/window_context.h"
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

class CubeApp : public Application {
 public:
  CubeApp() : command_{context()}, model_{context()} {}
  void MainLoop() override;

 private:
  void Init();
  void UpdateData(int frame, float frame_aspect);

  bool is_first_time = true;
  int current_frame_ = 0;
  common::Timer timer_;
  PerFrameCommand command_;
  Model model_;
  PushConstant push_constant_;
  std::unique_ptr<DepthStencilImage> depth_stencil_;
  std::unique_ptr<RenderPass> render_pass_;
};

} /* namespace */

void CubeApp::Init() {
  // window
  window_context_.Init("Cube");

  if (is_first_time) {
    is_first_time = false;

    // push constants
    push_constant_.Init(sizeof(Transformation), kNumFrameInFlight);
  }

  // depth stencil
  auto frame_size = window_context_.frame_size();
  depth_stencil_ = absl::make_unique<DepthStencilImage>(
      context(), window_context_.frame_size());

  // render pass
  render_pass_ = RenderPassBuilder::DefaultBuilder(
      context(), window_context_.swapchain(), *depth_stencil_)
          .Build(window_context_.swapchain(), *depth_stencil_);

  // model
  Model::TextureBindingMap bindings{};
  bindings[Model::ResourceType::kTextureDiffuse] = {
      /*binding_point=*/1,
      {{"external/resource/texture/statue.jpg"}},
  };
  model_.Init({{VK_SHADER_STAGE_VERTEX_BIT,
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
              {**render_pass_, /*subpass=*/0},
              frame_size, kNumFrameInFlight, /*is_opaque=*/true);

  // command buffer
  command_.Init(kNumFrameInFlight, &window_context_.queues());
}

void CubeApp::UpdateData(int frame, float frame_aspect) {
  const float elapsed_time = timer_.time_from_launch();
  glm::mat4 model = glm::rotate(glm::mat4{1.0f},
                                elapsed_time * glm::radians(90.0f),
                                glm::vec3{1.0f, 1.0f, 0.0f});
  glm::mat4 view = glm::lookAt(glm::vec3{3.0f}, glm::vec3{0.0f},
                               glm::vec3{0.0f, 0.0f, 1.0f});
  glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                    frame_aspect, 0.1f, 100.0f);
  *push_constant_.data<Transformation>(frame) = {proj * view * model};
}

void CubeApp::MainLoop() {
  Init();
  while (!window_context_.ShouldQuit()) {
    const VkExtent2D frame_size = window_context_.frame_size();
    const auto update_data = [this, frame_size](int frame) {
      UpdateData(frame, (float)frame_size.width / frame_size.height);
    };
    const auto draw_result = command_.Run(
        current_frame_, *window_context_.swapchain(), update_data,
        [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
          render_pass_->Run(
              command_buffer, framebuffer_index, frame_size,
              [this, &command_buffer]() {
                model_.Draw(command_buffer, current_frame_,
                            /*instance_count=*/1);
              });
        });

    if (draw_result != VK_SUCCESS || window_context_.ShouldRecreate()) {
      window_context_.Cleanup();
      command_.Cleanup();
      Init();
    }

    const auto frame_rate = timer_.frame_rate();
    if (frame_rate.has_value()) {
      std::cout << "Frame per second: " << frame_rate.value() << std::endl;
    }
    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
    window_context_.PollEvents();
  }
  window_context_.WaitIdle();  // wait for all async operations finish
}

} /* namespace cube */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, const char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<cube::CubeApp>();
}
