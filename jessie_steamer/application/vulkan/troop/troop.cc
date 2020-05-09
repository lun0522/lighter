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
  std::unique_ptr<troop::GeometryPass> geometry_pass_;
  std::unique_ptr<troop::LightingPass> lighting_pass_;
  std::unique_ptr<wrapper::vulkan::Image> depth_stencil_image_;
  std::unique_ptr<wrapper::vulkan::OffscreenImage> position_image_;
  std::unique_ptr<wrapper::vulkan::OffscreenImage> normal_image_;
  std::unique_ptr<wrapper::vulkan::OffscreenImage> diffuse_specular_image_;
};

} /* namespace */

TroopApp::TroopApp(const WindowContext::Config& window_config)
    : Application{"Troop", window_config} {
  using WindowKey = common::Window::KeyMap;
  using ControlKey = common::UserControlledCamera::ControlKey;

  /* Camera */
  common::Camera::Config config;
  config.position = glm::vec3{8.5f, 5.5f, 5.0f};
  config.look_at = glm::vec3{8.0f, 5.0f, 4.22f};

  const common::PerspectiveCamera::FrustumConfig frustum_config{
      /*field_of_view_y=*/45.0f, window_context().original_aspect_ratio()};

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
  geometry_pass_ = absl::make_unique<troop::GeometryPass>(
      window_context(), kNumFramesInFlight, /*model_scale=*/0.2,
      /*num_soldiers=*/glm::ivec2{5, 10},
      /*interval_between_soldiers=*/glm::vec2{1.7f, -1.0f});

  lighting_pass_ = absl::make_unique<troop::LightingPass>(
      &window_context(), kNumFramesInFlight,
      troop::LightingPass::LightCenterConfig{
          /*bound_x=*/{-3.0f, 9.8f}, /*bound_y=*/{3.2f, 4.2f},
          /*bound_z=*/{-12.0f, 3.0f}, /*increments=*/{0.0f, 0.0f, 2.0f},
      });
}

void TroopApp::Recreate() {
  /* Camera */
  camera_->SetCursorPos(window_context().window().GetCursorPos());

  /* Image */
  const VkExtent2D& frame_size = window_context().frame_size();
  depth_stencil_image_ =
      absl::make_unique<DepthStencilImage>(context(), frame_size);

  struct imageInfo {
    std::string name;
    bool high_precision;
    std::unique_ptr<OffscreenImage>* image;
  };
  imageInfo image_infos[]{
      {"Position", /*high_precision=*/true, &position_image_},
      {"Normal", /*high_precision=*/true, &normal_image_},
      {"Diffuse specular", /*high_precision=*/false, &diffuse_specular_image_},
  };

  const ImageSampler::Config sampler_config{VK_FILTER_NEAREST};
  for (auto& info : image_infos) {
    image::UsageInfo usage_info{std::move(info.name)};
    auto geometry_stage_usage = image::Usage::GetRenderTargetUsage();
    if (info.high_precision) {
      geometry_stage_usage.set_use_high_precision();
    }
    usage_info
        .AddUsage(kGeometryStage, geometry_stage_usage)
        .AddUsage(kLightingStage,
                  image::Usage::GetSampledInFragmentShaderUsage());
    *info.image = absl::make_unique<OffscreenImage>(
        context(), frame_size, common::kRgbaImageChannel,
        usage_info.GetAllUsages(), sampler_config);
  }

  /* Render pass */
  geometry_pass_->UpdateFramebuffer(*depth_stencil_image_, *position_image_,
                                    *normal_image_, *diffuse_specular_image_);
  lighting_pass_->UpdateFramebuffer(*depth_stencil_image_, *position_image_,
                                    *normal_image_, *diffuse_specular_image_);
}

void TroopApp::UpdateData(int frame) {
  geometry_pass_->UpdatePerFrameData(frame, camera_->camera());
  lighting_pass_->UpdatePerFrameData(frame, camera_->camera(),
                                     /*light_model_scale=*/0.1f);
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
          geometry_pass_->Draw(command_buffer, framebuffer_index,
                               current_frame_);
          lighting_pass_->Draw(command_buffer, framebuffer_index,
                               current_frame_);
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
