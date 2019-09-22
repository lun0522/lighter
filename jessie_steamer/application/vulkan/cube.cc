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
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/common/time.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
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

using common::file::GetResourcePath;
using common::file::GetShaderPath;

constexpr int kNumFrameInFlight = 2;

enum class SubpassIndex : int { kModel = 0, kText, kNumSubpass };

struct Transformation {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

class CubeApp : public Application {
 public:
  CubeApp();
  void MainLoop() override;

 private:
  void Recreate();
  void UpdateData(int frame, float frame_aspect);

  int current_frame_ = 0;
  common::Timer timer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<PushConstant> push_constant_;
  std::unique_ptr<Model> model_;
  std::unique_ptr<Image> depth_stencil_image_;
  std::unique_ptr<StaticText> static_text_;
  std::unique_ptr<DynamicText> dynamic_text_;
  std::unique_ptr<RenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
};

} /* namespace */

CubeApp::CubeApp() : Application{"Cube", WindowContext::Config{}} {
  // command buffer
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFrameInFlight);

  // push constant
  push_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(Transformation), kNumFrameInFlight);

  // render pass builder
  render_pass_builder_ = RenderPassBuilder::SimpleRenderPassBuilder(
      context(), static_cast<int>(SubpassIndex::kNumSubpass),
      window_context_.num_swapchain_image(),
      window_context_.multisampling_mode());

  // model
  ModelBuilder::TextureBindingMap bindings{};
  bindings[model::TextureType::kDiffuse] = {
      /*binding_point=*/1,
      {SharedTexture::SingleTexPath{GetResourcePath("texture/statue.jpg")}},
  };
  model_ =
      ModelBuilder{
          context(), kNumFrameInFlight, /*is_opaque=*/true,
          ModelBuilder::SingleMeshResource{GetResourcePath("model/cube.obj"),
              /*obj_index_base=*/1, bindings}}
          .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                       GetShaderPath("vulkan/simple_3d.vert.spv")})
          .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                       GetShaderPath("vulkan/simple_3d.frag.spv")})
          .set_push_constant({VK_SHADER_STAGE_VERTEX_BIT,
                              {{push_constant_.get(), /*offset=*/0}}})
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

void CubeApp::Recreate() {
  // depth stencil
  const VkExtent2D& frame_size = window_context_.frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
      context(), frame_size, window_context_.multisampling_mode());

  // render pass
  if (window_context_.multisampling_mode() != absl::nullopt) {
    render_pass_builder_->update_image(
        simple_render_pass::kMultisampleAttachmentIndex,
        [this](int index) -> const Image& {
          return window_context_.multisample_image();
        });
  }
  render_pass_ = (*render_pass_builder_)
      .set_framebuffer_size(frame_size)
      .update_image(simple_render_pass::kColorAttachmentIndex,
                    [this](int index) -> const Image& {
                      return window_context_.swapchain_image(index);
                    })
      .update_image(simple_render_pass::kDepthStencilAttachmentIndex,
                    [this](int index) -> const Image& {
                      return *depth_stencil_image_;
                    })
      .Build();

  // model
  model_->Update(frame_size, depth_stencil_image_->sample_count(),
                 *render_pass_, static_cast<int>(SubpassIndex::kModel));

  // text
  static_text_->Update(frame_size, *render_pass_,
                       static_cast<int>(SubpassIndex::kText));
  dynamic_text_->Update(frame_size, *render_pass_,
                        static_cast<int>(SubpassIndex::kText));
}

void CubeApp::UpdateData(int frame, float frame_aspect) {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();
  glm::mat4 model = glm::rotate(glm::mat4{1.0f},
                                elapsed_time * glm::radians(90.0f),
                                glm::vec3{1.0f, 1.0f, 0.0f});
  glm::mat4 view = glm::lookAt(glm::vec3{3.0f}, glm::vec3{0.0f},
                               glm::vec3{0.0f, 0.0f, 1.0f});
  glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                    frame_aspect, 0.1f, 100.0f);
  *push_constant_->HostData<Transformation>(frame) = {proj * view * model};
}

void CubeApp::MainLoop() {
  Recreate();
  while (window_context_.CheckEvents()) {
    timer_.Tick();

    const VkExtent2D& frame_size = window_context_.frame_size();
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
    const auto draw_result = command_->Run(
        current_frame_, window_context_.swapchain(), update_data,
        [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
          render_pass_->Run(command_buffer, framebuffer_index, render_ops);
        });

    if (draw_result != VK_SUCCESS || window_context_.ShouldRecreate()) {
      window_context_.Recreate();
      Recreate();
    }

    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
  }
  context()->WaitIdle();  // wait for all async operations finish
}

} /* namespace cube */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<cube::CubeApp>(argc, argv);
}
