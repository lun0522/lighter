#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

#include "lighter/scene/model.h"
#include "lighter/scene/skybox.h"
#include "lighter/scene/util.h"

namespace lighter::example {
namespace {

using renderer::ir::AccessLocation;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::ImageHandle;
using renderer::ir::MultisamplingMode;
using renderer::ir::PassBuilder;
using renderer::ir::PassContext;
using renderer::ir::Renderer;
using renderer::ir::RenderGraph;
using scene::Model;
using scene::ModelBuilder;

constexpr int kWindowIndex = 0;
constexpr int kNumFramesInFlight = 2;
constexpr int kObjFileIndexBase = 1;

// Consistent with uniform blocks defined in shaders.
struct NanosuitVertTrans {
  ALIGN_MAT4 glm::mat4 view_model;
  ALIGN_MAT4 glm::mat4 proj_view_model;
  ALIGN_MAT4 glm::mat4 view_model_inv_trs;
};

struct NanosuitFragTrans {
  ALIGN_MAT4 glm::mat4 view_inv;
};

class NanosuitExample {
 public:
  NanosuitExample(const glm::ivec2& screen_size,
                  MultisamplingMode multisampling_mode)
      : multisampling_mode_{multisampling_mode},
        window_{"Nanosuit", screen_size} {
    using base::file::GetResourcePath;
    using TextureType = ModelBuilder::TextureType;

    renderer_ = scene::CreateRenderer("Nanosuit Example", {&window_});

    const float aspect_ratio = window_.original_aspect_ratio();
    scene::Camera::Config camera_config;
    camera_config.position = glm::vec3{0.0f, 4.0f, -12.0f};
    camera_config.look_at = glm::vec3{0.0f, 4.0f, 0.0f};

    scene::camera_control::Config control_config;
    control_config.move_speed = 1.0f;
    control_config.lock_center = camera_config.look_at;

    camera_ = scene::UserControlledPerspectiveCamera::Create(
        control_config, camera_config,
        {/*field_of_view_y=*/45.0f, aspect_ratio});
    scene::RegisterCameraControls(window_, timer_, *camera_, &should_quit_);

    const std::array uniform_usages{
        BufferUsage::GetUniformBufferUsage(AccessLocation::kVertexShader)};
    for (auto& buffer : nanosuit_vert_uniforms_) {
      buffer = renderer_->CreateBuffer(
          Buffer::UpdateRate::kHigh, sizeof(NanosuitVertTrans), uniform_usages);
    }

    const ModelBuilder::CubemapPath skybox_path{
        .directory = GetResourcePath("texture/tidepool/right.tga",
                                     /*want_directory_path=*/true),
        .files = {"right.tga", "left.tga", "top.tga", "bottom.tga", "back.tga",
                  "front.tga"},
    };

    nanosuit_model_ =
        ModelBuilder{
            *renderer_, "Nanosuit", aspect_ratio,
            ModelBuilder::MultiMeshResource{
                /*model_path=*/GetResourcePath("model/nanosuit/nanosuit.obj"),
                /*texture_dir=*/
                GetResourcePath("model/nanosuit/nanosuit.obj",
                                /*want_directory_path=*/true),
            }}
            .AddSharedTexture(TextureType::kCubemap, skybox_path)
            .AddTextureBindingPoint(TextureType::kDiffuse, /*binding=*/1)
            .AddTextureBindingPoint(TextureType::kSpecular, /*binding=*/2)
            .AddTextureBindingPoint(TextureType::kReflection, /*binding=*/3)
            .AddTextureBindingPoint(TextureType::kCubemap, /*binding=*/4)
            .SetShader(
                renderer::ir::shader_stage::VERTEX,
                base::file::GetShaderBinaryPath("nanosuit/nanosuit.vert"))
            .SetShader(
                renderer::ir::shader_stage::FRAGMENT,
                base::file::GetShaderBinaryPath("nanosuit/nanosuit.frag"))
            .Build();

    skybox_ =
        std::make_unique<scene::Skybox>(*renderer_, aspect_ratio, skybox_path);
  }

  // This class is neither copyable nor movable.
  NanosuitExample(const NanosuitExample&) = delete;
  NanosuitExample& operator=(const NanosuitExample&) = delete;

  void MainLoop() {
    camera_->SetCursorPos(window_.GetCursorPos());
    RenderGraph graph;

    while (!should_quit_ && !window_.ShouldQuit()) {
      window_.ProcessUserInputs();
      timer_.Tick();

      const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();
      glm::mat4 model{1.0f};
      model = glm::rotate(model, elapsed_time * glm::radians(90.0f),
                          glm::vec3{0.0f, 1.0f, 0.0f});
      model = glm::scale(model, glm::vec3{0.5f});

      const scene::Camera& cam = camera_->camera();
      const glm::mat4 view = cam.GetViewMatrix();
      const glm::mat4 proj = cam.GetProjectionMatrix();
      const glm::mat4 view_model = view * model;

      const NanosuitVertTrans vert_trans{
          view_model,
          proj * view_model,
          glm::transpose(glm::inverse(view_model)),
      };
      const NanosuitFragTrans frag_trans{glm::inverse(view)};
      const glm::mat4 skybox_proj_view_model = proj * cam.GetSkyboxViewMatrix();

      graph.Reset();
      const ImageHandle backbuffer = graph.ImportSwapchainImage(kWindowIndex);
      graph.AddGraphicsPass(
          "NanosuitAndSkybox",
          [&](PassBuilder& builder) {
            scene::SetupMainPassAttachments(builder, backbuffer,
                                            multisampling_mode_,
                                            /*use_depth=*/true);
          },
          [&](PassContext& ctx) {
            const glm::ivec2 frame_size = ctx.GetImageExtent(backbuffer);
            const Buffer& vert_uniform = *nanosuit_vert_uniforms_[frame_slot_];
            vert_uniform.CopyToDevice(std::array{Buffer::CopyInfo{
                &vert_trans, sizeof(NanosuitVertTrans), /*offset=*/0}});

            nanosuit_model_->BindPipeline(ctx, frame_size);
            ctx.BindBuffer(/*binding=*/0, vert_uniform);
            ctx.PushConstants(renderer::ir::shader_stage::FRAGMENT,
                              /*offset=*/0, sizeof(NanosuitFragTrans),
                              &frag_trans);
            nanosuit_model_->Draw(ctx);

            skybox_->Draw(ctx, frame_size, skybox_proj_view_model);
          });

      if (!renderer_->RenderFrame(kWindowIndex, graph)) {
        camera_->SetCursorPos(window_.GetCursorPos());
      }
      frame_slot_ = (frame_slot_ + 1) % kNumFramesInFlight;
      // Camera is not activated until the first frame is displayed.
      camera_->SetActivity(true);
    }
    renderer_->WaitIdle();
  }

 private:
  const MultisamplingMode multisampling_mode_;
  bool should_quit_ = false;
  int frame_slot_ = 0;
  base::FrameTimer timer_;
  base::Window window_;
  std::unique_ptr<scene::UserControlledPerspectiveCamera> camera_;
  std::unique_ptr<Renderer> renderer_;
  std::array<std::unique_ptr<Buffer>, kNumFramesInFlight>
      nanosuit_vert_uniforms_;
  std::unique_ptr<Model> nanosuit_model_;
  std::unique_ptr<scene::Skybox> skybox_;
};

}  // namespace
}  // namespace lighter::example

int main(int argc, char* argv[]) {
  return lighter::scene::RunApplication<lighter::example::NanosuitExample>(
      argc, argv, glm::ivec2{800, 600},
      lighter::renderer::ir::MultisamplingMode::kDecent);
}
