//
//  troop.cc
//
//  Created by Pujun Lun on 5/4/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <memory>
#include <string>

#include "lighter/application/vulkan/troop/geometry_pass.h"
#include "lighter/application/vulkan/troop/lighting_pass.h"
#include "lighter/application/vulkan/util.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace {

using namespace renderer;
using namespace renderer::vulkan;

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
  std::unique_ptr<common::UserControlledPerspectiveCamera> camera_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<troop::GeometryPass> geometry_pass_;
  std::unique_ptr<troop::LightingPass> lighting_pass_;
  std::unique_ptr<renderer::vulkan::Image> depth_stencil_image_;
  std::unique_ptr<renderer::vulkan::OffscreenImage> position_image_;
  std::unique_ptr<renderer::vulkan::OffscreenImage> normal_image_;
  std::unique_ptr<renderer::vulkan::OffscreenImage> diffuse_specular_image_;
};

} /* namespace */

TroopApp::TroopApp(const WindowContext::Config& window_config)
    : Application{"Troop", window_config} {
  using WindowKey = common::Window::KeyMap;
  using ControlKey = common::camera_control::Key;

  /* Camera */
  common::Camera::Config camera_config;
  camera_config.position = glm::vec3{8.5f, 5.5f, 5.0f};
  camera_config.look_at = glm::vec3{8.0f, 5.0f, 4.22f};

  const common::PerspectiveCamera::FrustumConfig frustum_config{
      /*field_of_view_y=*/45.0f, window_context().original_aspect_ratio()};

  camera_ = common::UserControlledPerspectiveCamera::Create(
      /*control_config=*/{}, camera_config, frustum_config);

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
  command_ = std::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

  /* Render pass */
  geometry_pass_ = std::make_unique<troop::GeometryPass>(
      &window_context(), kNumFramesInFlight, /*model_scale=*/0.2,
      /*num_soldiers=*/glm::ivec2{5, 10},
      /*interval_between_soldiers=*/glm::vec2{1.7f, -1.0f});

  lighting_pass_ = std::make_unique<troop::LightingPass>(
      &window_context(), kNumFramesInFlight,
      troop::LightingPass::LightCenterConfig{
          /*bound_x=*/{-3.0f, 9.8f}, /*bound_y=*/{3.2f, 4.2f},
          /*bound_z=*/{-12.0f, 3.0f}, /*increments=*/{0.0f, 0.0f, 2.0f},
      });
}

void TroopApp::Recreate() {
  enum SubpassIndex {
    kGeometrySubpassIndex = 0,
    kLightingSubpassIndex,
  };

  /* Camera */
  camera_->SetCursorPos(window_context().window().GetCursorPos());

  /* Image */
  const VkExtent2D& frame_size = window_context().frame_size();
  depth_stencil_image_ =
      std::make_unique<DepthStencilImage>(context(), frame_size);

  struct imageInfo {
    std::unique_ptr<OffscreenImage>* image;
    bool high_precision;
  };
  imageInfo image_infos[]{
      {&position_image_, /*high_precision=*/true},
      {&normal_image_, /*high_precision=*/true},
      {&diffuse_specular_image_, /*high_precision=*/false},
  };

  const ImageSampler::Config sampler_config{VK_FILTER_NEAREST};
  for (int i = 0; i < 3; i++) {
    auto& info = image_infos[i];
    ImageUsageHistory usage_history;
    usage_history
        .AddUsage(kGeometrySubpassIndex,
                  ImageUsage::GetRenderTargetUsage(/*attachment_location=*/i))
        .AddUsage(kLightingSubpassIndex,
                  ImageUsage::GetSampledInFragmentShaderUsage());
    *info.image = std::make_unique<OffscreenImage>(
        context(), frame_size, common::image::kRgbaImageChannel,
        usage_history.GetAllUsages(), sampler_config, info.high_precision);
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
} /* namespace lighter */

int main(int argc, char* argv[]) {
  using namespace lighter::application::vulkan;
  return AppMain<TroopApp>(argc, argv, WindowContext::Config{});
}
