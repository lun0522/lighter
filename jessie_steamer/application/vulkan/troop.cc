//
//  troop.cc
//
//  Created by Pujun Lun on 5/4/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/util.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;

enum SubpassIndex {
  kModelSubpassIndex = 0,
  kNumSubpasses,
};

constexpr int kNumFramesInFlight = 2;
constexpr int kNumNanosuitsX = 5;
constexpr int kNumNanosuitsZ = 10;
constexpr float kIntervalX = 8.0f;
constexpr float kIntervalZ = -5.0f;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Transformation {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

/* END: Consistent with uniform blocks defined in shaders. */

class TroopApp : public Application {
 public:
  explicit TroopApp(const WindowContext::Config& config);

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
  std::unique_ptr<StaticPerInstanceBuffer> center_data_;
  std::unique_ptr<PushConstant> trans_constant_;
  std::unique_ptr<NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
  std::unique_ptr<Image> depth_stencil_image_;
  std::unique_ptr<Model> nanosuit_model_;
};

} /* namespace */

TroopApp::TroopApp(const WindowContext::Config& window_config)
    : Application{"Troop", window_config} {
  using common::file::GetResourcePath;
  using common::file::GetVkShaderPath;
  using WindowKey = common::Window::KeyMap;
  using ControlKey = common::UserControlledCamera::ControlKey;
  using TextureType = ModelBuilder::TextureType;

  const float original_aspect_ratio = window_context().original_aspect_ratio();

  /* Camera */
  common::Camera::Config config;
  config.position = glm::vec3{8.5f, 5.5f, 5.0f};
  config.look_at = glm::vec3{8.0f, 5.0f, 4.2f};

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

  /* Push constant */
  trans_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(Transformation), kNumFramesInFlight);

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/true,
      /*num_transparent_subpasses=*/0,
      /*num_overlay_subpasses=*/0,
  };
  render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
      context(), subpass_config,
      /*num_framebuffers=*/window_context().num_swapchain_images(),
      window_context().use_multisampling(),
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kPresentToScreen);

  /* Vertex buffer */
  std::vector<glm::vec3> centers;
  centers.reserve(kNumNanosuitsX * kNumNanosuitsZ);
  for (int x = 0; x < kNumNanosuitsX; ++x) {
    for (int z = 0; z < kNumNanosuitsZ; ++z) {
      centers.push_back(glm::vec3{kIntervalX * static_cast<float>(x), 0.0f,
                                  kIntervalZ * static_cast<float>(z)});
    }
  }
  center_data_ = absl::make_unique<StaticPerInstanceBuffer>(
      context(), centers,
      pipeline::GetVertexAttribute<common::Vertex3DPosOnly>());

  /* Model */
  nanosuit_model_ = ModelBuilder{
      context(), "Nanosuit", kNumFramesInFlight, original_aspect_ratio,
      ModelBuilder::MultiMeshResource{
          GetResourcePath("model/nanosuit/nanosuit.obj"),
          GetResourcePath("model/nanosuit")}}
      .AddTextureBindingPoint(TextureType::kDiffuse, /*binding_point=*/1)
      .AddTextureBindingPoint(TextureType::kSpecular, /*binding_point=*/2)
      .AddTextureBindingPoint(TextureType::kReflection, /*binding_point=*/3)
      .AddPerInstanceBuffer(center_data_.get())
      .AddUniformBinding(
          VK_SHADER_STAGE_VERTEX_BIT,
          /*bindings=*/{{/*binding_point=*/0, /*array_length=*/1}})
      .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
      .AddPushConstant(trans_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetVkShaderPath("troop/troop.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetVkShaderPath("troop/troop.frag"))
      .Build();
}

void TroopApp::Recreate() {
  /* Camera */
  camera_->SetCursorPos(window_context().window().GetCursorPos());

  /* Depth image */
  const VkExtent2D& frame_size = window_context().frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
      context(), frame_size, window_context().multisampling_mode());

  /* Render pass */
  (*render_pass_builder_)
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
    render_pass_builder_->UpdateAttachmentImage(
        render_pass_builder_->multisample_attachment_index(),
        [this](int framebuffer_index) -> const Image& {
          return window_context().multisample_image();
        });
  }
  render_pass_ = render_pass_builder_->Build();

  /* Model */
  nanosuit_model_->Update(/*is_object_opaque=*/true, frame_size,
                          window_context().sample_count(),
                          *render_pass_, kModelSubpassIndex);
}

void TroopApp::UpdateData(int frame) {
  const common::Camera& camera = camera_->camera();
  trans_constant_->HostData<Transformation>(frame)->proj_view_model =
      camera.GetProjectionMatrix() * camera.GetViewMatrix() *
      glm::scale(glm::mat4{1.0f}, glm::vec3{0.2f});
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
          render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
              [this](const VkCommandBuffer& command_buffer) {
                nanosuit_model_->Draw(
                    command_buffer, current_frame_,
                    /*instance_count=*/kNumNanosuitsX * kNumNanosuitsZ);
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
