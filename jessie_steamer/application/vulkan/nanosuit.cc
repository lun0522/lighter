//
//  nanosuit.cc
//
//  Created by Pujun Lun on 3/25/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <vector>

#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/common/camera.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;

constexpr int kNumFramesInFlight = 2;
constexpr int kObjFileIndexBase = 1;

enum SubpassIndex {
  kModelSubpassIndex = 0,
  kNumSubpasses,
};

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
  std::unique_ptr<UniformBuffer> nanosuit_vert_uniform_;
  std::unique_ptr<PushConstant> nanosuit_frag_constant_;
  std::unique_ptr<PushConstant> skybox_constant_;
  std::unique_ptr<NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
  std::unique_ptr<Image> depth_stencil_image_;
  std::unique_ptr<Model> nanosuit_model_;
  std::unique_ptr<Model> skybox_model_;
};

} /* namespace */

NanosuitApp::NanosuitApp(const WindowContext::Config& window_config)
    : Application{"Nanosuit", window_config} {
  using common::file::GetResourcePath;
  using common::file::GetVkShaderPath;
  using WindowKey = common::Window::KeyMap;
  using ControlKey = common::UserControlledCamera::ControlKey;
  using TextureType = ModelBuilder::TextureType;

  const float original_aspect_ratio = window_context().original_aspect_ratio();

  /* Camera */
  common::Camera::Config config;
  config.position = glm::vec3{0.0f, 4.0f, -12.0f};
  config.look_at = glm::vec3{0.0f, 4.0f, 0.0f};
  const common::PerspectiveCamera::PersConfig pers_config{
      /*field_of_view=*/45.0f, original_aspect_ratio};
  auto pers_camera =
      absl::make_unique<common::PerspectiveCamera>(config, pers_config);

  common::UserControlledCamera::ControlConfig control_config;
  control_config.lock_center = config.look_at;
  camera_ = absl::make_unique<common::UserControlledCamera>(
      control_config, std::move(pers_camera));

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

  /* Uniform buffer and push constants */
  nanosuit_vert_uniform_ = absl::make_unique<UniformBuffer>(
      context(), sizeof(NanosuitVertTrans), kNumFramesInFlight);
  nanosuit_frag_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(NanosuitFragTrans), kNumFramesInFlight);
  skybox_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(SkyboxTrans), kNumFramesInFlight);

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

  /* Model */
  const SharedTexture::CubemapPath skybox_path{
      /*directory=*/GetResourcePath("texture/tidepool"),
      /*files=*/{
          "right.tga", "left.tga",
          "top.tga", "bottom.tga",
          "back.tga", "front.tga",
      },
  };

  nanosuit_model_ = ModelBuilder{
      context(), "nanosuit", kNumFramesInFlight, original_aspect_ratio,
      ModelBuilder::MultiMeshResource{
          GetResourcePath("model/nanosuit/nanosuit.obj"),
          GetResourcePath("model/nanosuit")}}
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
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT, GetVkShaderPath("nanosuit.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT, GetVkShaderPath("nanosuit.frag"))
      .Build();

  skybox_model_ = ModelBuilder{
      context(), "skybox", kNumFramesInFlight, original_aspect_ratio,
      ModelBuilder::SingleMeshResource{
          GetResourcePath("model/skybox.obj"), kObjFileIndexBase,
          /*tex_source_map=*/{{TextureType::kCubemap, {skybox_path}}},
      }}
      .AddTextureBindingPoint(TextureType::kCubemap, /*binding_point=*/1)
      .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
      .AddPushConstant(skybox_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT, GetVkShaderPath("skybox.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT, GetVkShaderPath("skybox.frag"))
      .Build();
}

void NanosuitApp::Recreate() {
  /* Camera */
  camera_->SetCursorPos(window_context().window().GetCursorPos());

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

  /* Model */
  constexpr bool kIsObjectOpaque = true;
  const VkSampleCountFlagBits sample_count = window_context().sample_count();
  nanosuit_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                          *render_pass_, kModelSubpassIndex);
  skybox_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                        *render_pass_, kModelSubpassIndex);
}

void NanosuitApp::UpdateData(int frame) {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(90.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  model = glm::scale(model, glm::vec3{0.5f});

  const common::Camera& camera = camera_->camera();
  const glm::mat4& view = camera.view();
  const glm::mat4& proj = camera.projection();
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

    const std::vector<RenderPass::RenderOp> render_ops{
        [this](const VkCommandBuffer& command_buffer) {
          nanosuit_model_->Draw(command_buffer, current_frame_,
                                /*instance_count=*/1);
          skybox_model_->Draw(command_buffer, current_frame_,
                              /*instance_count=*/1);
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
  return AppMain<NanosuitApp>(argc, argv, WindowContext::Config{});
}
