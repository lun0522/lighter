//
//  cube.cc
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/common/time.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/text.h"
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

enum class SubpassIndex : int { kModel = 0, kText, kNumSubpass };

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct Transformation {
  alignas(16) glm::mat4 proj_view_model;
};

class CubeApp : public Application {
 public:
  CubeApp() : command_{context()} {}
  void MainLoop() override;

 private:
  void Init();
  void UpdateData(int frame, float frame_aspect);

  bool is_first_time = true;
  int current_frame_ = 0;
  common::Timer timer_;
  PerFrameCommand command_;
  PushConstant push_constant_;
  std::unique_ptr<Model> model_;
  std::unique_ptr<DepthStencilImage> depth_stencil_;
  std::unique_ptr<StaticText> static_text_;
  std::unique_ptr<DynamicText> dynamic_text_;
  std::unique_ptr<RenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
};

} /* namespace */

void CubeApp::Init() {
  // window
  window_context_.Init("Cube");

  // depth stencil
  auto frame_size = window_context_.frame_size();
  depth_stencil_ = absl::make_unique<DepthStencilImage>(context(), frame_size);

  if (is_first_time) {
    is_first_time = false;

    // push constants
    push_constant_.Init(sizeof(Transformation), kNumFrameInFlight);

    // render pass builder
    render_pass_builder_ = RenderPassBuilder::SimpleRenderPassBuilder(
        context(), static_cast<int>(SubpassIndex::kNumSubpass), *depth_stencil_,
        window_context_.swapchain().num_image(),
        /*get_swapchain_image=*/[this](int index) -> const Image& {
          return window_context_.swapchain().image(index);
        });

    // model
    ModelBuilder::TextureBindingMap bindings{};
    bindings[model::ResourceType::kTextureDiffuse] = {
        /*binding_point=*/1,
        {SharedTexture::SingleTexPath{"external/resource/texture/statue.jpg"}},
    };
    model_ =
        ModelBuilder{
            context(), kNumFrameInFlight, /*is_opaque=*/true,
            ModelBuilder::SingleMeshResource{"external/resource/model/cube.obj",
            /*obj_index_base=*/1, bindings}}
        .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                     "jessie_steamer/shader/vulkan/simple_3d.vert.spv"})
        .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                     "jessie_steamer/shader/vulkan/simple_3d.frag.spv"})
        .add_push_constant({VK_SHADER_STAGE_VERTEX_BIT,
                            {{&push_constant_, /*offset=*/0}}})
        .Build();

    // text
    constexpr CharLoader::Font kFont = Text::Font::kGeorgia;
    constexpr int kFontHeight = 50;
    static_text_ = absl::make_unique<StaticText>(
        context(), kNumFrameInFlight,
        std::vector<std::string>{"FPS: "}, kFont, kFontHeight);
    dynamic_text_ = absl::make_unique<DynamicText>(
        context(), kNumFrameInFlight,
        std::vector<std::string>{"01234567890"}, kFont, kFontHeight);
  }

  // render pass
  render_pass_ = (*render_pass_builder_)
      .set_framebuffer_size(frame_size)
      .update_attachment(/*index=*/1, [this](int index) -> const Image& {
        return *depth_stencil_;
      })
      .Build();

  // model
  model_->Update(frame_size, *render_pass_,
                 static_cast<int>(SubpassIndex::kModel));

  // text
  static_text_->Update(frame_size, *render_pass_,
                       static_cast<int>(SubpassIndex::kText));
  dynamic_text_->Update(frame_size, *render_pass_,
                        static_cast<int>(SubpassIndex::kText));

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
    std::vector<RenderPass::RenderOp> render_ops{
        [&](const VkCommandBuffer& command_buffer) {
          model_->Draw(command_buffer, current_frame_, /*instance_count=*/1);
        },
        [&](const VkCommandBuffer& command_buffer) {
          const glm::vec3 kColor{0.7f};
          constexpr float kAlpha = 1.0f;
          constexpr float kHeight = 0.05f;
          constexpr float kBaseX = 0.04f;
          constexpr float kBaseY = 0.05f;
          glm::vec2 boundary = static_text_->Draw(
              command_buffer, current_frame_, frame_size, /*text_index=*/0,
              kColor, kAlpha, kHeight, kBaseX, kBaseY, Text::Align::kLeft);
          dynamic_text_->Draw(
              command_buffer, current_frame_, frame_size,
              std::to_string(timer_.frame_rate()),
              kColor, kAlpha, kHeight, boundary.y, kBaseY, Text::Align::kLeft);
        },
    };
    const auto draw_result = command_.Run(
        current_frame_, *window_context_.swapchain(), update_data,
        [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
          render_pass_->Run(command_buffer, framebuffer_index, render_ops);
        });

    if (draw_result != VK_SUCCESS || window_context_.ShouldRecreate()) {
      window_context_.Cleanup();
      command_.Cleanup();
      Init();
    }

    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
    window_context_.PollEvents();
    timer_.Tick();
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
