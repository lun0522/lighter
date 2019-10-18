//
//  planet.cc
//
//  Created by Pujun Lun on 4/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <numeric>
#include <random>
#include <vector>

#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/common/camera.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;

using std::vector;

constexpr int kNumAsteroidRings = 3;
constexpr int kNumFramesInFlight = 2;
constexpr int kObjFileIndexBase = 1;

enum SubpassIndex {
  kModelSubpassIndex = 0,
  kNumSubpasses,
};

// Contains the data shared by all vertices of an asteroid.
struct Asteroid {
  float theta;
  float radius;
  glm::mat4 model;
};

/* BEGIN: Consistent with structs used in shaders. */

struct Light {
  ALIGN_VEC4 glm::vec4 direction_time;
};

struct PlanetTrans {
  ALIGN_MAT4 glm::mat4 model;
  ALIGN_MAT4 glm::mat4 proj_view;
};

struct SkyboxTrans {
  ALIGN_MAT4 glm::mat4 proj;
  ALIGN_MAT4 glm::mat4 view;
};

/* END: Consistent with structs used in shaders. */

class PlanetApp : public Application {
 public:
  explicit PlanetApp(const WindowContext::Config& config);

  // Overrides.
  void MainLoop() override;

 private:
  // Recreates the swapchain and associated resources.
  void Recreate();

  // Populates 'num_asteroids_' and 'per_asteroid_data_'.
  void GenAsteroidModels();

  // Updates per-frame data.
  void UpdateData(int frame);

  bool should_quit_ = false;
  int current_frame_ = 0;
  int num_asteroids_ = -1;
  common::Timer timer_;
  std::unique_ptr<common::UserControlledCamera> camera_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<PerInstanceBuffer> per_asteroid_data_;
  std::unique_ptr<UniformBuffer> light_uniform_;
  std::unique_ptr<PushConstant> planet_constant_;
  std::unique_ptr<PushConstant> skybox_constant_;
  std::unique_ptr<NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
  std::unique_ptr<Image> depth_stencil_image_;
  std::unique_ptr<Model> planet_model_, asteroid_model_, skybox_model_;
};

} /* namespace */

PlanetApp::PlanetApp(const WindowContext::Config& window_config)
    : Application{"Planet", window_config} {
  using common::file::GetResourcePath;
  using common::file::GetShaderPath;
  using WindowKey = common::Window::KeyMap;
  using ControlKey = common::UserControlledCamera::ControlKey;
  using TextureType = ModelBuilder::TextureType;

  /* Camera */
  common::Camera::Config config;
  config.position = glm::vec3{1.6f, -5.1f, -5.9f};
  config.look_at = glm::vec3{-2.4f, -0.8f, 0.0f};
  camera_ = absl::make_unique<common::UserControlledCamera>(
      config, common::UserControlledCamera::ControlConfig{});

  /* Window */
  (*window_context_.mutable_window())
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
  light_uniform_ = absl::make_unique<UniformBuffer>(
      context(), sizeof(Light), kNumFramesInFlight);
  planet_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(PlanetTrans), kNumFramesInFlight);
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
      /*num_framebuffers=*/window_context_.num_swapchain_images(),
      /*present_to_screen=*/true, window_context_.multisampling_mode());

  /* Model */
  planet_model_ =
      ModelBuilder{
          context(), kNumFramesInFlight,
          ModelBuilder::SingleMeshResource{
              GetResourcePath("model/sphere.obj"), kObjFileIndexBase,
              /*tex_source_map=*/{{
                  TextureType::kDiffuse,
                  {SharedTexture::SingleTexPath{
                       GetResourcePath("texture/planet.png")}},
              }}
          }}
          .AddTextureBindingPoint(TextureType::kDiffuse, /*binding_point=*/2)
          .AddUniformBinding(
              VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{{/*binding_point=*/1, /*array_length=*/1}})
          .AddUniformBuffer(/*binding_point=*/1, *light_uniform_)
          .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
          .AddPushConstant(planet_constant_.get(), /*target_offset=*/0)
          .AddShader(VK_SHADER_STAGE_VERTEX_BIT,
                     GetShaderPath("vulkan/planet.vert.spv"))
          .AddShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                     GetShaderPath("vulkan/planet.frag.spv"))
          .Build();

  GenAsteroidModels();
  asteroid_model_ =
      ModelBuilder{
          context(), kNumFramesInFlight,
          ModelBuilder::MultiMeshResource{
              GetResourcePath("model/rock/rock.obj"),
              GetResourcePath("model/rock"),
          }}
          .AddTextureBindingPoint(TextureType::kDiffuse, /*binding_point=*/2)
          .AddPerInstanceBuffer(per_asteroid_data_.get())
          .AddUniformBinding(
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{{/*binding_point=*/1, /*array_length=*/1}})
          .AddUniformBuffer(/*binding_point=*/1, *light_uniform_)
          .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
          .AddPushConstant(planet_constant_.get(), /*target_offset=*/0)
          .AddShader(VK_SHADER_STAGE_VERTEX_BIT,
                     GetShaderPath("vulkan/asteroid.vert.spv"))
          .AddShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                     GetShaderPath("vulkan/planet.frag.spv"))
          .Build();

  const SharedTexture::CubemapPath skybox_path{
      /*directory=*/GetResourcePath("texture/universe"),
      /*files=*/{
          "PositiveX.jpg", "NegativeX.jpg",
          "PositiveY.jpg", "NegativeY.jpg",
          "PositiveZ.jpg", "NegativeZ.jpg",
      },
  };

  skybox_model_ =
      ModelBuilder{
          context(), kNumFramesInFlight,
          ModelBuilder::SingleMeshResource{
              GetResourcePath("model/skybox.obj"), kObjFileIndexBase,
              {{TextureType::kCubemap, {skybox_path}}},
          }}
          .AddTextureBindingPoint(TextureType::kCubemap, /*binding_point=*/1)
          .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
          .AddPushConstant(skybox_constant_.get(), /*target_offset=*/0)
          .AddShader(VK_SHADER_STAGE_VERTEX_BIT,
                     GetShaderPath("vulkan/skybox.vert.spv"))
          .AddShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                     GetShaderPath("vulkan/skybox.frag.spv"))
          .Build();
}

void PlanetApp::Recreate() {
  /* Camera */
  camera_->Calibrate(window_context_.window().GetScreenSize(),
                     window_context_.window().GetCursorPos());

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
  render_pass_ = (**render_pass_builder_).Build();

  /* Model */
  constexpr bool kIsObjectOpaque = true;
  const VkSampleCountFlagBits sample_count = window_context_.sample_count();
  planet_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                        *render_pass_, kModelSubpassIndex);
  asteroid_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                          *render_pass_, kModelSubpassIndex);
  skybox_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                        *render_pass_, kModelSubpassIndex);
}

void PlanetApp::GenAsteroidModels() {
  const std::array<int, kNumAsteroidRings> num_asteroid = {300, 500, 700};
  const std::array<float, kNumAsteroidRings> radii = {6.0f, 12.0f,  18.0f};

  // Randomly generate rotation, radius and scale for each asteroid.
  std::random_device device;
  std::mt19937 rand_gen{device()};
  std::uniform_real_distribution<float> axis_gen{0.0f, 1.0f};
  std::uniform_real_distribution<float> angle_gen{0.0f, 360.0f};
  std::uniform_real_distribution<float> radius_gen{-1.5f, 1.5f};
  std::uniform_real_distribution<float> scale_gen{1.0f, 3.0f};

  num_asteroids_ = static_cast<int>(std::accumulate(
      num_asteroid.begin(), num_asteroid.end(), 0));
  vector<Asteroid> asteroids;
  asteroids.reserve(num_asteroids_);

  for (int ring = 0; ring < kNumAsteroidRings; ++ring) {
    for (int i = 0; i < num_asteroid[ring]; ++i) {
      glm::mat4 model{1.0f};
      model = glm::rotate(model, glm::radians(angle_gen(rand_gen)),
                          glm::vec3{axis_gen(rand_gen), axis_gen(rand_gen),
                                    axis_gen(rand_gen)});
      model = glm::scale(model, glm::vec3{scale_gen(rand_gen) * 0.02f});

      asteroids.emplace_back(Asteroid{
          /*theta=*/glm::radians(angle_gen(rand_gen)),
          /*radius=*/radii[ring] + radius_gen(rand_gen),
          model,
      });
    }
  }

  vector<VertexBuffer::Attribute> per_instance_attribs{
      {offsetof(Asteroid, theta), VK_FORMAT_R32_SFLOAT},
      {offsetof(Asteroid, radius), VK_FORMAT_R32_SFLOAT},
  };
  per_instance_attribs.reserve(6);
  uint32_t attrib_offset = offsetof(Asteroid, model);
  // The mat4 will be bound as 4 vec4.
  for (int i = 0; i < 4; ++i) {
    per_instance_attribs.emplace_back(
        VertexBuffer::Attribute{attrib_offset, VK_FORMAT_R32G32B32A32_SFLOAT});
    attrib_offset += sizeof(glm::vec4);
  }
  per_asteroid_data_ = absl::make_unique<PerInstanceBuffer>(
      context(), asteroids, std::move(per_instance_attribs));
}

void PlanetApp::UpdateData(int frame) {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();

  const glm::vec3 light_dir{glm::sin(elapsed_time * 0.6f), -0.3f,
                            glm::cos(elapsed_time * 0.6f)};
  *light_uniform_->HostData<Light>(frame) =
      {glm::vec4{light_dir, elapsed_time}};
  light_uniform_->Flush(frame);

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(5.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  const glm::mat4& view = camera_->view();
  const glm::mat4& proj = camera_->projection();
  *planet_constant_->HostData<PlanetTrans>(frame) = {model, proj * view};
  *skybox_constant_->HostData<SkyboxTrans>(frame) = {proj, view};
}

void PlanetApp::MainLoop() {
  const auto update_data = [this](int frame) { UpdateData(frame); };

  Recreate();
  while (!should_quit_ && window_context_.CheckEvents()) {
    timer_.Tick();

    const vector<RenderPass::RenderOp> render_ops{
        [&](const VkCommandBuffer& command_buffer) {
          planet_model_->Draw(command_buffer, current_frame_,
                              /*instance_count=*/1);
          asteroid_model_->Draw(command_buffer, current_frame_,
                                static_cast<uint32_t>(num_asteroids_));
          skybox_model_->Draw(command_buffer, current_frame_,
                              /*instance_count=*/1);
        },
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
    // Camera is not activated until first frame is displayed.
    camera_->SetActivity(true);
  }
  context()->WaitIdle();
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<PlanetApp>(argc, argv, WindowContext::Config{});
}
