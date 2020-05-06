//
//  troop.cc
//
//  Created by Pujun Lun on 5/4/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <string>

#include "jessie_steamer/application/vulkan/troop/geometry_pass.h"
#include "jessie_steamer/application/vulkan/troop/lighting_pass.h"
#include "jessie_steamer/application/vulkan/util.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;
using GeometryPass = troop::GeometryPass;

enum ProcessingStage {
  kGeometryStage,
  kLightingStage,
};

constexpr int kNumFramesInFlight = 2;

class TroopApp : public Application {
 public:
  explicit TroopApp(const WindowContext::Config& config);

  // This class is neither copyable nor movable.
  TroopApp(const TroopApp&) = delete;
  TroopApp& operator=(const TroopApp&) = delete;

  // Overrides.
  void MainLoop() override;

 private:
  // Recreates the swapchain and associated resources.
  void Recreate();

  // Updates per-frame data.
  void UpdateData(int frame);

  bool should_quit_ = false;
  int current_frame_ = 0;
  common::FrameTimer timer_;
  std::unique_ptr<common::UserControlledCamera> camera_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<DeferredShadingRenderPassBuilder>
      geometry_render_pass_builder_;
  // TODO: Move following objects to GeometryPass?
  std::unique_ptr<RenderPass> geometry_render_pass_;
  std::unique_ptr<GeometryPass> geometry_pass_;
  std::unique_ptr<wrapper::vulkan::Image> depth_stencil_image_;
  std::unique_ptr<wrapper::vulkan::Image> position_image_;
  std::unique_ptr<wrapper::vulkan::Image> normal_image_;
  std::unique_ptr<wrapper::vulkan::Image> diffuse_specular_image_;
};

} /* namespace */

TroopApp::TroopApp(const WindowContext::Config& window_config)
    : Application{"Troop", window_config} {
  using WindowKey = common::Window::KeyMap;
  using ControlKey = common::UserControlledCamera::ControlKey;

  const float original_aspect_ratio = window_context().original_aspect_ratio();

  /* Camera */
  common::Camera::Config config;
  config.position = glm::vec3{8.5f, 5.5f, 5.0f};
  config.look_at = glm::vec3{8.0f, 5.0f, 4.25f};

  const common::PerspectiveCamera::FrustumConfig frustum_config{
      /*field_of_view_y=*/45.0f, original_aspect_ratio};

  camera_ = absl::make_unique<common::UserControlledCamera>(
      common::UserControlledCamera::ControlConfig{},
      absl::make_unique<common::PerspectiveCamera>(config, frustum_config));

  /* Window */
  (*mutable_window_context()->mutable_window())
      .SetCursorHidden(true)
      .RegisterMoveCursorCallback([this](double x_pos, double y_pos) {
        camera_->DidMoveCursor(x_pos, y_pos);
      })
      .RegisterScrollCallback([this](double x_pos, double y_pos) {
        camera_->DidScroll(y_pos, 1.0f, 60.0f);
      })
      .RegisterPressKeyCallback(WindowKey::kUp, [this]() {
        camera_->DidPressKey(ControlKey::kUp,
                             timer_.GetElapsedTimeSinceLastFrame());
      })
      .RegisterPressKeyCallback(WindowKey::kDown, [this]() {
        camera_->DidPressKey(ControlKey::kDown,
                             timer_.GetElapsedTimeSinceLastFrame());
      })
      .RegisterPressKeyCallback(WindowKey::kLeft, [this]() {
        camera_->DidPressKey(ControlKey::kLeft,
                             timer_.GetElapsedTimeSinceLastFrame());
      })
      .RegisterPressKeyCallback(WindowKey::kRight, [this]() {
        camera_->DidPressKey(ControlKey::kRight,
                             timer_.GetElapsedTimeSinceLastFrame());
      })
      .RegisterPressKeyCallback(WindowKey::kEscape,
                                [this]() { should_quit_ = true; });

  /* Command buffer */
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

  /* Render pass */
  geometry_render_pass_builder_ =
      absl::make_unique<DeferredShadingRenderPassBuilder>(
      context(), /*num_framebuffers=*/window_context().num_swapchain_images(),
      GeometryPass::kNumColorAttachments);

  /* Pipeline */
  geometry_pass_ = absl::make_unique<GeometryPass>(
      context(), kNumFramesInFlight, original_aspect_ratio, /*model_scale=*/0.2,
      /*num_soldiers=*/glm::ivec2{5, 10},
      /*interval_between_soldiers=*/glm::vec2{8.0f, -5.0f});
}

void TroopApp::Recreate() {
  /* Camera */
  camera_->SetCursorPos(window_context().window().GetCursorPos());

  /* Image */
  const VkExtent2D& frame_size = window_context().frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
      context(), frame_size, /*mode=*/absl::nullopt);

  struct imageInfo {
    std::string name;
    bool high_precision;
    std::unique_ptr<Image>* image;
  };
  imageInfo image_infos[]{
      {"Position", /*high_precision=*/true, &position_image_},
      {"Normal", /*high_precision=*/true, &normal_image_},
      {"Diffuse specular", /*high_precision=*/false, &diffuse_specular_image_},
  };

  const ImageSampler::Config sampler_config{VK_FILTER_NEAREST};
  for (auto& info : image_infos) {
    image::UsageInfo usage_info{std::move(info.name)};
    usage_info
        .AddUsage(kGeometryStage,
                  image::Usage::GetRenderTargetUsage(info.high_precision))
        .AddUsage(kLightingStage,
                  image::Usage::GetSampledInFragmentShaderUsage());
    *info.image = absl::make_unique<OffscreenImage>(
        context(), frame_size, common::kRgbaImageChannel,
        usage_info.GetAllUsages(), sampler_config);
  }

  /* Render pass */
  const int color_attachments_index_base =
      geometry_render_pass_builder_->color_attachments_index_base();
  (*geometry_render_pass_builder_)
      .UpdateAttachmentImage(
          geometry_render_pass_builder_->depth_attachment_index(),
          [this](int framebuffer_index) -> const Image& {
            return *depth_stencil_image_;
          })
      .UpdateAttachmentImage(
          color_attachments_index_base + GeometryPass::kPositionImageIndex,
          [this](int framebuffer_index) -> const Image& {
            return *position_image_;
          })
      .UpdateAttachmentImage(
          color_attachments_index_base + GeometryPass::kNormalImageIndex,
          [this](int framebuffer_index) -> const Image& {
            return *normal_image_;
          })
      .UpdateAttachmentImage(
          color_attachments_index_base +
              GeometryPass::kDiffuseSpecularImageIndex,
          [this](int framebuffer_index) -> const Image& {
            return *diffuse_specular_image_;
          });
  geometry_render_pass_ = geometry_render_pass_builder_->Build();

  /* Pipeline */
  geometry_pass_->UpdateFramebuffer(frame_size, *geometry_render_pass_,
                                    /*subpass_index=*/0);
}

void TroopApp::UpdateData(int frame) {
  geometry_pass_->UpdatePerFrameData(frame, camera_->camera());
}

void TroopApp::MainLoop() {
  const auto update_data = [this](int frame) { UpdateData(frame); };

  Recreate();
  while (!should_quit_ && mutable_window_context()->CheckEvents()) {
    timer_.Tick();

    const auto draw_result = command_->Run(
        current_frame_, window_context().swapchain(), update_data,
        [this](const VkCommandBuffer& command_buffer,
               uint32_t framebuffer_index) {
          geometry_render_pass_->Run(
              command_buffer, framebuffer_index, /*render_ops=*/{
                  [this](const VkCommandBuffer& command_buffer) {
                    geometry_pass_->Draw(command_buffer, current_frame_);
                  },
              });
        });

    if (draw_result.has_value() || window_context().ShouldRecreate()) {
      mutable_window_context()->Recreate();
      Recreate();
    }
    current_frame_ = (current_frame_ + 1) % kNumFramesInFlight;
    // Camera is not activated until first frame is displayed.
    camera_->SetActivity(true);
  }
  mutable_window_context()->OnExit();
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<TroopApp>(argc, argv, WindowContext::Config{});
}
