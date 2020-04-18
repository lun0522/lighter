//
//  cube.cc
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/wrapper/vulkan/text.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;

enum SubpassIndex {
  kModelSubpassIndex = 0,
  kTextSubpassIndex,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kTextSubpassIndex,
};

constexpr int kNumFramesInFlight = 2;
constexpr int kObjFileIndexBase = 1;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Transformation {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

/* END: Consistent with uniform blocks defined in shaders. */

class CubeApp : public Application {
 public:
  explicit CubeApp(const WindowContext::Config& config);

  // Overrides.
  void MainLoop() override;

 private:
  // Recreates the swapchain and associated resources.
  void Recreate();

  // Updates per-frame data.
  void UpdateData(int frame);

  int current_frame_ = 0;
  common::FrameTimer timer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<PushConstant> trans_constant_;
  std::unique_ptr<NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
  std::unique_ptr<Image> depth_stencil_image_;
  std::unique_ptr<Model> cube_model_;
  std::unique_ptr<StaticText> static_text_;
  std::unique_ptr<DynamicText> dynamic_text_;
};

} /* namespace */

CubeApp::CubeApp(const WindowContext::Config& window_config)
    : Application{"Cube", window_config} {
  // Prevent shaders from being auto released.
  ModelBuilder::AutoReleaseShaderPool shader_pool;

  const float original_aspect_ratio = window_context().original_aspect_ratio();

  /* Command buffer */
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

  /* Push constant */
  trans_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(Transformation), kNumFramesInFlight);

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/true,
      /*num_transparent_subpasses=*/0,
      /*num_overlay_subpasses=*/kNumOverlaySubpasses,
  };
  render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
      context(), subpass_config,
      /*num_framebuffers=*/window_context().num_swapchain_images(),
      window_context().use_multisampling(),
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kPresentToScreen);

  // TODO: Add utils for resource paths and shader paths.
  /* Model */
  cube_model_ = ModelBuilder{
      context(), "Cube", kNumFramesInFlight, original_aspect_ratio,
      ModelBuilder::SingleMeshResource{
          common::file::GetResourcePath("model/cube.obj"), kObjFileIndexBase,
          /*tex_source_map=*/{{
              ModelBuilder::TextureType::kDiffuse,
              {SharedTexture::SingleTexPath{
                   common::file::GetResourcePath("texture/statue.jpg")}},
          }}
      }}
      .AddTextureBindingPoint(ModelBuilder::TextureType::kDiffuse,
                              /*binding_point=*/1)
      .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
      .AddPushConstant(trans_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("cube/cube.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("cube/cube.frag"))
      .Build();

  /* Text */
  constexpr auto kFont = Text::Font::kGeorgia;
  constexpr int kFontHeight = 100;
  static_text_ = absl::make_unique<StaticText>(
      context(), kNumFramesInFlight, original_aspect_ratio,
      std::vector<std::string>{"FPS: "}, kFont, kFontHeight);
  dynamic_text_ = absl::make_unique<DynamicText>(
      context(), kNumFramesInFlight, original_aspect_ratio,
      std::vector<std::string>{"01234567890"}, kFont, kFontHeight);
}

void CubeApp::Recreate() {
  // Prevent shaders from being auto released.
  ModelBuilder::AutoReleaseShaderPool shader_pool;

  /* Depth image */
  const VkExtent2D& frame_size = window_context().frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
      context(), frame_size, window_context().multisampling_mode());

  /* Render pass */
  (*render_pass_builder_->mutable_builder())
      .UpdateAttachmentImage(
          render_pass_builder_->color_attachment_index(),
          [this](int framebuffer_index) -> const Image& {
            return window_context().swapchain_image(framebuffer_index);
          })
      .UpdateAttachmentImage(
          render_pass_builder_->depth_attachment_index(),
          [this](int framebuffer_index) -> const Image& {
            return *depth_stencil_image_;
          });
  if (render_pass_builder_->has_multisample_attachment()) {
    render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
        render_pass_builder_->multisample_attachment_index(),
        [this](int framebuffer_index) -> const Image& {
          return window_context().multisample_image();
        });
  }
  render_pass_ = (*render_pass_builder_)->Build();

  /* Model and text */
  const VkSampleCountFlagBits sample_count = window_context().sample_count();
  cube_model_->Update(/*is_object_opaque=*/true, frame_size, sample_count,
                      *render_pass_, kModelSubpassIndex);
  static_text_->Update(frame_size, sample_count,
                       *render_pass_, kTextSubpassIndex);
  dynamic_text_->Update(frame_size, sample_count,
                        *render_pass_, kTextSubpassIndex);
}

void CubeApp::UpdateData(int frame) {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();
  const glm::mat4 model = glm::rotate(glm::mat4{1.0f},
                                      elapsed_time * glm::radians(90.0f),
                                      glm::vec3{1.0f, 1.0f, 0.0f});
  const glm::mat4 view = glm::lookAt(glm::vec3{3.0f}, glm::vec3{0.0f},
                                     glm::vec3{0.0f, 0.0f, 1.0f});
  const glm::mat4 proj = glm::perspective(
      glm::radians(45.0f), window_context().original_aspect_ratio(),
      0.1f, 100.0f);
  *trans_constant_->HostData<Transformation>(frame) = {proj * view * model};
}

void CubeApp::MainLoop() {
  constexpr float kTextHeight = 0.05f;
  constexpr float kTextBaseX = 0.04f;
  constexpr float kTextBaseY = 0.05f;
  constexpr float kTextAlpha = 0.5f;
  const glm::vec3 text_color{1.0f};

  const auto update_data = [this](int frame) { UpdateData(frame); };

  Recreate();
  while (mutable_window_context()->CheckEvents()) {
    timer_.Tick();

    const glm::vec2 boundary = static_text_->AddText(
        /*text_index=*/0, kTextHeight, kTextBaseX, kTextBaseY,
        Text::Align::kLeft);
    dynamic_text_->AddText(std::to_string(timer_.frame_rate()), kTextHeight,
                           boundary.y, kTextBaseY, Text::Align::kLeft);

    const std::vector<RenderPass::RenderOp> render_ops{
        [this](const VkCommandBuffer& command_buffer) {
          cube_model_->Draw(command_buffer, current_frame_,
                            /*instance_count=*/1);
        },
        [this, &text_color](const VkCommandBuffer& command_buffer) {
          static_text_->Draw(command_buffer, current_frame_,
                             text_color, kTextAlpha);
          dynamic_text_->Draw(command_buffer, current_frame_,
                              text_color, kTextAlpha);
        },
    };
    const auto draw_result = command_->Run(
        current_frame_, window_context().swapchain(), update_data,
        [this, &render_ops](const VkCommandBuffer& command_buffer,
                            uint32_t framebuffer_index) {
          render_pass_->Run(command_buffer, framebuffer_index, render_ops);
        });

    if (draw_result.has_value() || window_context().ShouldRecreate()) {
      mutable_window_context()->Recreate();
      Recreate();
    }
    current_frame_ = (current_frame_ + 1) % kNumFramesInFlight;
  }
  mutable_window_context()->OnExit();
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<CubeApp>(argc, argv, WindowContext::Config{});
}
