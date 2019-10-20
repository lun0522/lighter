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

using std::vector;

constexpr int kNumFramesInFlight = 2;
constexpr int kObjFileIndexBase = 1;

enum SubpassIndex {
  kModelSubpassIndex = 0,
  kTextSubpassIndex,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kTextSubpassIndex,
};

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
  void UpdateData(int frame, float frame_aspect);

  int current_frame_ = 0;
  common::Timer timer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<PushConstant> push_constant_;
  std::unique_ptr<NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
  std::unique_ptr<Image> depth_stencil_image_;
  std::unique_ptr<Model> model_;
  std::unique_ptr<StaticText> static_text_;
  std::unique_ptr<DynamicText> dynamic_text_;
};

} /* namespace */

CubeApp::CubeApp(const WindowContext::Config& window_config)
    : Application{"Cube", window_config} {
  using common::file::GetResourcePath;
  using common::file::GetShaderPath;

  // Prevent shaders from being auto released.
  ModelBuilder::AutoReleaseShaderPool shader_pool;

  /* Command buffer */
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

  /* Push constant */
  push_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(Transformation), kNumFramesInFlight);

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/true,
      /*num_transparent_subpasses=*/0,
      /*num_overlay_subpasses=*/kNumOverlaySubpasses,
  };
  render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
      context(), subpass_config,
      /*num_framebuffers=*/window_context_.num_swapchain_images(),
      /*present_to_screen=*/true, window_context_.multisampling_mode());

  // TODO: Add utils for resource paths and shader paths.
  /* Model */
  model_ = ModelBuilder{
      context(), "cube", kNumFramesInFlight,
      ModelBuilder::SingleMeshResource{
          GetResourcePath("model/cube.obj"), kObjFileIndexBase,
          /*tex_source_map=*/{{
              ModelBuilder::TextureType::kDiffuse,
              {SharedTexture::SingleTexPath{
                   GetResourcePath("texture/statue.jpg")}},
          }}
      }}
      .AddTextureBindingPoint(ModelBuilder::TextureType::kDiffuse,
                              /*binding_point=*/1)
      .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
      .AddPushConstant(push_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderPath("vulkan/simple_3d.vert.spv"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderPath("vulkan/simple_3d.frag.spv"))
      .Build();

  /* Text */
  constexpr auto kFont = Text::Font::kGeorgia;
  constexpr int kFontHeight = 100;
  static_text_ = absl::make_unique<StaticText>(
      context(), kNumFramesInFlight,
      vector<std::string>{"FPS: "}, kFont, kFontHeight);
  dynamic_text_ = absl::make_unique<DynamicText>(
      context(), kNumFramesInFlight,
      vector<std::string>{"01234567890"}, kFont, kFontHeight);
}

void CubeApp::Recreate() {
  // Prevent shaders from being auto released.
  ModelBuilder::AutoReleaseShaderPool shader_pool;

  /* Depth image */
  const VkExtent2D& frame_size = window_context_.frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
      context(), frame_size, window_context_.multisampling_mode());

  /* Render pass */
  (*render_pass_builder_->mutable_builder())
      .UpdateAttachmentImage(
          render_pass_builder_->color_attachment_index(),
          [this](int framebuffer_index) -> const Image& {
            return window_context_.swapchain_image(framebuffer_index);
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
          return window_context_.multisample_image();
        });
  }
  render_pass_ = (*render_pass_builder_)->Build();

  /* Model and text */
  const VkSampleCountFlagBits sample_count = window_context_.sample_count();
  model_->Update(/*is_object_opaque=*/true, frame_size, sample_count,
                 *render_pass_, kModelSubpassIndex);
  static_text_->Update(frame_size, sample_count,
                       *render_pass_, kTextSubpassIndex);
  dynamic_text_->Update(frame_size, sample_count,
                        *render_pass_, kTextSubpassIndex);
}

void CubeApp::UpdateData(int frame, float frame_aspect) {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();
  const glm::mat4 model = glm::rotate(glm::mat4{1.0f},
                                      elapsed_time * glm::radians(90.0f),
                                      glm::vec3{1.0f, 1.0f, 0.0f});
  const glm::mat4 view = glm::lookAt(glm::vec3{3.0f}, glm::vec3{0.0f},
                                     glm::vec3{0.0f, 0.0f, 1.0f});
  const glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                          frame_aspect, 0.1f, 100.0f);
  *push_constant_->HostData<Transformation>(frame) = {proj * view * model};
}

void CubeApp::MainLoop() {
  Recreate();
  while (window_context_.CheckEvents()) {
    timer_.Tick();

    const VkExtent2D& frame_size = window_context_.frame_size();
    const vector<RenderPass::RenderOp> render_ops{
        [&](const VkCommandBuffer& command_buffer) {
          model_->Draw(command_buffer, current_frame_, /*instance_count=*/1);
        },
        [&](const VkCommandBuffer& command_buffer) {
          const glm::vec3 kColor{1.0f};
          constexpr float kAlpha = 0.5f;
          constexpr float kHeight = 0.05f;
          constexpr float kBaseX = 0.04f;
          constexpr float kBaseY = 0.05f;
          const glm::vec2 boundary =
              static_text_->Draw(command_buffer, current_frame_, frame_size,
                                 /*text_index=*/0, kColor, kAlpha,
                                 kHeight, kBaseX, kBaseY, Text::Align::kLeft);
          dynamic_text_->Draw(command_buffer, current_frame_, frame_size,
                              std::to_string(timer_.frame_rate()), kColor,
                              kAlpha, kHeight, boundary.y, kBaseY,
                              Text::Align::kLeft);
        },
    };
    const auto update_data = [this, frame_size](int frame) {
      UpdateData(frame, (float)frame_size.width / frame_size.height);
    };
    const auto draw_result = command_->Run(
        current_frame_, window_context_.swapchain(), update_data,
        [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
          render_pass_->Run(command_buffer, framebuffer_index, render_ops);
        });

    if (draw_result.has_value() || window_context_.ShouldRecreate()) {
      window_context_.Recreate();
      Recreate();
    }
    current_frame_ = (current_frame_ + 1) % kNumFramesInFlight;
  }
  window_context_.OnExit();
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<CubeApp>(argc, argv, WindowContext::Config{});
}
