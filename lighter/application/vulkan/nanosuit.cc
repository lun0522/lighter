//
//  nanosuit.cc
//
//  Created by Pujun Lun on 3/25/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <vector>

#include "lighter/application/vulkan/util.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace {

using namespace renderer::vulkan;

enum SubpassIndex {
  kModelSubpassIndex = 0,
  kNumSubpasses,
};

constexpr int kNumFramesInFlight = 2;
constexpr int kObjFileIndexBase = 1;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct NanosuitVertTrans {
  ALIGN_MAT4 glm::mat4 view_model;
  ALIGN_MAT4 glm::mat4 proj_view_model;
  ALIGN_MAT4 glm::mat4 view_model_inv_trs;
};

struct NanosuitFragTrans {
  ALIGN_MAT4 glm::mat4 view_inv;
};

struct SkyboxTrans {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

/* END: Consistent with uniform blocks defined in shaders. */

class NanosuitApp : public Application {
 public:
  explicit NanosuitApp(const WindowContext::Config& config);

  // This class is neither copyable nor movable.
  NanosuitApp(const NanosuitApp&) = delete;
  NanosuitApp& operator=(const NanosuitApp&) = delete;

  // Overrides.
  void MainLoop() override;

 private:
  // Recreates the swapchain and associated resources.
  void Recreate();

  // Updates per-frame data.
  void UpdateData(int frame);

  // Accessors.
  const RenderPass& render_pass() const {
    return render_pass_manager_->render_pass();
  }

  bool should_quit_ = false;
  int current_frame_ = 0;
  common::FrameTimer timer_;
  std::unique_ptr<common::UserControlledPerspectiveCamera> camera_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<UniformBuffer> nanosuit_vert_uniform_;
  std::unique_ptr<PushConstant> nanosuit_frag_constant_;
  std::unique_ptr<PushConstant> skybox_constant_;
  std::unique_ptr<Model> nanosuit_model_;
  std::unique_ptr<Model> skybox_model_;
  std::unique_ptr<OnScreenRenderPassManager> render_pass_manager_;
};

} /* namespace */

NanosuitApp::NanosuitApp(const WindowContext::Config& window_config)
    : Application{"Nanosuit", window_config} {
  using common::file::GetResourcePath;
  using WindowKey = common::Window::KeyMap;
  using ControlKey = common::camera_control::Key;
  using TextureType = ModelBuilder::TextureType;

  const float original_aspect_ratio = window_context().original_aspect_ratio();

  /* Camera */
  common::Camera::Config camera_config;
  camera_config.position = glm::vec3{0.0f, 4.0f, -12.0f};
  camera_config.look_at = glm::vec3{0.0f, 4.0f, 0.0f};

  common::camera_control::Config control_config;
  control_config.move_speed = 1.0f;
  control_config.lock_center = camera_config.look_at;

  const common::PerspectiveCamera::FrustumConfig frustum_config{
      /*field_of_view_y=*/45.0f, original_aspect_ratio};

  camera_ = common::UserControlledPerspectiveCamera::Create(
      control_config, camera_config, frustum_config);

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

  /* Uniform buffer and push constant */
  nanosuit_vert_uniform_ = std::make_unique<UniformBuffer>(
      context(), sizeof(NanosuitVertTrans), kNumFramesInFlight);
  nanosuit_frag_constant_ = std::make_unique<PushConstant>(
      context(), sizeof(NanosuitFragTrans), kNumFramesInFlight);
  skybox_constant_ = std::make_unique<PushConstant>(
      context(), sizeof(SkyboxTrans), kNumFramesInFlight);

  /* Model */
  const SharedTexture::CubemapPath skybox_path{
      /*directory=*/
      GetResourcePath("texture/tidepool/right.tga",
                      /*want_directory_path=*/true),
      /*files=*/{
          "right.tga", "left.tga",
          "top.tga", "bottom.tga",
          "back.tga", "front.tga",
      },
  };

  nanosuit_model_ = ModelBuilder{
      context(), "Nanosuit", kNumFramesInFlight, original_aspect_ratio,
      ModelBuilder::MultiMeshResource{
          /*model_path=*/GetResourcePath("model/nanosuit/nanosuit.obj"),
          /*texture_dir=*/
          GetResourcePath("model/nanosuit/nanosuit.obj",
                          /*want_directory_path=*/true)}}
      .AddSharedTexture(TextureType::kCubemap, skybox_path)
      .AddTextureBindingPoint(TextureType::kDiffuse, /*binding_point=*/1)
      .AddTextureBindingPoint(TextureType::kSpecular, /*binding_point=*/2)
      .AddTextureBindingPoint(TextureType::kReflection, /*binding_point=*/3)
      .AddTextureBindingPoint(TextureType::kCubemap, /*binding_point=*/4)
      .AddUniformBinding(
           VK_SHADER_STAGE_VERTEX_BIT,
           /*bindings=*/{{/*binding_point=*/0, /*array_length=*/1}})
      .AddUniformBuffer(/*binding_point=*/0, *nanosuit_vert_uniform_)
      .SetPushConstantShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT)
      .AddPushConstant(nanosuit_frag_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderBinaryPath("nanosuit/nanosuit.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderBinaryPath("nanosuit/nanosuit.frag"))
      .Build();

  skybox_model_ = ModelBuilder{
      context(), "Skybox", kNumFramesInFlight, original_aspect_ratio,
      ModelBuilder::SingleMeshResource{
          GetResourcePath("model/skybox.obj"), kObjFileIndexBase,
          /*tex_source_map=*/{{TextureType::kCubemap, {skybox_path}}},
      }}
      .AddTextureBindingPoint(TextureType::kCubemap, /*binding_point=*/1)
      .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
      .AddPushConstant(skybox_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderBinaryPath("shared/skybox.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderBinaryPath("shared/skybox.frag"))
      .Build();

  /* Render pass */
  render_pass_manager_ = std::make_unique<OnScreenRenderPassManager>(
      &window_context(),
      NaiveRenderPass::SubpassConfig{
          kNumSubpasses, /*first_transparent_subpass=*/std::nullopt,
          /*first_overlay_subpass=*/std::nullopt});
}

void NanosuitApp::Recreate() {
  /* Camera */
  camera_->SetCursorPos(window_context().window().GetCursorPos());

  /* Render pass */
  render_pass_manager_->RecreateRenderPass();

  /* Model */
  constexpr bool kIsObjectOpaque = true;
  const VkExtent2D& frame_size = window_context().frame_size();
  const VkSampleCountFlagBits sample_count = window_context().sample_count();
  nanosuit_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                          render_pass(), kModelSubpassIndex);
  skybox_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                        render_pass(), kModelSubpassIndex);
}

void NanosuitApp::UpdateData(int frame) {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(90.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  model = glm::scale(model, glm::vec3{0.5f});

  const common::Camera& camera = camera_->camera();
  const glm::mat4 view = camera.GetViewMatrix();
  const glm::mat4 proj = camera.GetProjectionMatrix();
  const glm::mat4 view_model = view * model;

  *nanosuit_vert_uniform_->HostData<NanosuitVertTrans>(frame) = {
      view_model,
      proj * view_model,
      glm::transpose(glm::inverse(view_model)),
  };
  nanosuit_vert_uniform_->Flush(frame);

  *nanosuit_frag_constant_->HostData<NanosuitFragTrans>(frame) =
      {glm::inverse(view)};
  skybox_constant_->HostData<SkyboxTrans>(frame)->proj_view_model =
      proj * camera.GetSkyboxViewMatrix();
}

void NanosuitApp::MainLoop() {
  const auto update_data = [this](int frame) { UpdateData(frame); };

  Recreate();
  while (!should_quit_ && mutable_window_context()->CheckEvents()) {
    timer_.Tick();

    const auto draw_result = command_->Run(
        current_frame_, window_context().swapchain(), update_data,
        [this](const VkCommandBuffer& command_buffer,
               uint32_t framebuffer_index) {
          render_pass().Run(command_buffer, framebuffer_index, /*render_ops=*/{
              [this](const VkCommandBuffer& command_buffer) {
                nanosuit_model_->Draw(command_buffer, current_frame_,
                                      /*instance_count=*/1);
                skybox_model_->Draw(command_buffer, current_frame_,
                                    /*instance_count=*/1);
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
} /* namespace lighter */

int main(int argc, char* argv[]) {
  using namespace lighter::application::vulkan;
  return AppMain<NanosuitApp>(argc, argv, WindowContext::Config{});
}
