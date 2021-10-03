//
//  planet.cc
//
//  Created by Pujun Lun on 4/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
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

constexpr int kNumAsteroidRings = 3;
constexpr int kNumFramesInFlight = 2;
constexpr int kObjFileIndexBase = 1;

/* BEGIN: Consistent with vertex input attributes defined in shaders. */

struct Asteroid {
  // Returns vertex input attributes.
  static std::vector<common::VertexAttribute> GetVertexAttributes() {
    std::vector<common::VertexAttribute> attributes;
    common::data::AppendVertexAttributes<glm::vec1>(
        attributes, offsetof(Asteroid, theta));
    common::data::AppendVertexAttributes<glm::vec1>(
        attributes, offsetof(Asteroid, radius));
    common::data::AppendVertexAttributes<glm::mat4>(
        attributes, offsetof(Asteroid, model));
    return attributes;
  }

  float theta;
  float radius;
  glm::mat4 model;
};

/* END: Consistent with vertex input attributes defined in shaders. */

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Light {
  ALIGN_VEC4 glm::vec4 direction_time;
};

struct PlanetTrans {
  ALIGN_MAT4 glm::mat4 model;
  ALIGN_MAT4 glm::mat4 proj_view;
};

struct SkyboxTrans {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

/* END: Consistent with uniform blocks defined in shaders. */

class PlanetApp : public Application {
 public:
  explicit PlanetApp(const WindowContext::Config& config);

  // This class is neither copyable nor movable.
  PlanetApp(const PlanetApp&) = delete;
  PlanetApp& operator=(const PlanetApp&) = delete;

  // Overrides.
  void MainLoop() override;

 private:
  // Recreates the swapchain and associated resources.
  void Recreate();

  // Populates 'num_asteroids_' and 'per_asteroid_data_'.
  void GenerateAsteroidModels();

  // Updates per-frame data.
  void UpdateData(int frame);

  // Accessors.
  const RenderPass& render_pass() const {
    return render_pass_manager_->render_pass();
  }

  bool should_quit_ = false;
  int current_frame_ = 0;
  std::optional<int> num_asteroids_;
  common::FrameTimer timer_;
  std::unique_ptr<common::UserControlledPerspectiveCamera> camera_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<StaticPerInstanceBuffer> per_asteroid_data_;
  std::unique_ptr<UniformBuffer> light_uniform_;
  std::unique_ptr<PushConstant> planet_constant_;
  std::unique_ptr<PushConstant> skybox_constant_;
  std::unique_ptr<Model> planet_model_;
  std::unique_ptr<Model> asteroid_model_;
  std::unique_ptr<Model> skybox_model_;
  std::unique_ptr<OnScreenRenderPassManager> render_pass_manager_;
};

} /* namespace */

PlanetApp::PlanetApp(const WindowContext::Config& window_config)
    : Application{"Planet", window_config} {
  using common::file::GetResourcePath;
  using WindowKey = common::Window::KeyMap;
  using ControlKey = common::camera_control::Key;
  using TextureType = ModelBuilder::TextureType;

  const float original_aspect_ratio = window_context().original_aspect_ratio();

  /* Camera */
  common::Camera::Config camera_config;
  camera_config.position = glm::vec3{1.6f, -5.1f, -5.9f};
  camera_config.look_at = glm::vec3{-2.4f, -0.8f, 0.0f};
  const common::PerspectiveCamera::FrustumConfig frustum_config{
      /*field_of_view_y=*/45.0f, original_aspect_ratio};
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

  /* Uniform buffer and push constant */
  light_uniform_ = std::make_unique<UniformBuffer>(
      context(), sizeof(Light), kNumFramesInFlight);
  planet_constant_ = std::make_unique<PushConstant>(
      context(), sizeof(PlanetTrans), kNumFramesInFlight);
  skybox_constant_ = std::make_unique<PushConstant>(
      context(), sizeof(SkyboxTrans), kNumFramesInFlight);

  /* Model */
  planet_model_ = ModelBuilder{
      context(), "Planet", kNumFramesInFlight, original_aspect_ratio,
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
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderBinaryPath("planet/planet.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderBinaryPath("planet/planet.frag"))
      .Build();

  GenerateAsteroidModels();
  asteroid_model_ = ModelBuilder{
      context(), "Asteroid", kNumFramesInFlight, original_aspect_ratio,
      ModelBuilder::MultiMeshResource{
          /*model_path=*/GetResourcePath("model/rock/rock.obj"),
          /*texture_dir=*/
          GetResourcePath("model/rock/rock.obj", /*want_directory_path=*/true)}}
      .AddTextureBindingPoint(TextureType::kDiffuse, /*binding_point=*/2)
      .AddPerInstanceBuffer(per_asteroid_data_.get())
      .AddUniformBinding(
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          /*bindings=*/{{/*binding_point=*/1, /*array_length=*/1}})
      .AddUniformBuffer(/*binding_point=*/1, *light_uniform_)
      .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
      .AddPushConstant(planet_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderBinaryPath("planet/asteroid.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderBinaryPath("planet/planet.frag"))
      .Build();

  const SharedTexture::CubemapPath skybox_path{
      /*directory=*/
      GetResourcePath("texture/universe/PositiveX.jpg",
                      /*want_directory_path=*/true),
      /*files=*/{
          "PositiveX.jpg", "NegativeX.jpg",
          "PositiveY.jpg", "NegativeY.jpg",
          "PositiveZ.jpg", "NegativeZ.jpg",
      },
  };

  skybox_model_ = ModelBuilder{
      context(), "Skybox", kNumFramesInFlight, original_aspect_ratio,
      ModelBuilder::SingleMeshResource{
          GetResourcePath("model/skybox.obj"), kObjFileIndexBase,
          {{TextureType::kCubemap, {skybox_path}}},
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

void PlanetApp::Recreate() {
  /* Camera */
  camera_->SetCursorPos(window_context().window().GetCursorPos());

  /* Render pass */
  render_pass_manager_->RecreateRenderPass();

  /* Model */
  constexpr bool kIsObjectOpaque = true;
  const VkExtent2D& frame_size = window_context().frame_size();
  const VkSampleCountFlagBits sample_count = window_context().sample_count();
  planet_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                        render_pass(), kModelSubpassIndex);
  asteroid_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                          render_pass(), kModelSubpassIndex);
  skybox_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                        render_pass(), kModelSubpassIndex);
}

void PlanetApp::GenerateAsteroidModels() {
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
  std::vector<Asteroid> asteroids;
  asteroids.reserve(num_asteroids_.value());

  for (int ring = 0; ring < kNumAsteroidRings; ++ring) {
    for (int i = 0; i < num_asteroid[ring]; ++i) {
      glm::mat4 model{1.0f};
      model = glm::rotate(model, glm::radians(angle_gen(rand_gen)),
                          glm::vec3{axis_gen(rand_gen), axis_gen(rand_gen),
                                    axis_gen(rand_gen)});
      model = glm::scale(model, glm::vec3{scale_gen(rand_gen) * 0.02f});

      asteroids.push_back(Asteroid{
          /*theta=*/glm::radians(angle_gen(rand_gen)),
          /*radius=*/radii[ring] + radius_gen(rand_gen),
          model,
      });
    }
  }

  per_asteroid_data_ = std::make_unique<StaticPerInstanceBuffer>(
      context(), asteroids, pipeline::GetVertexAttributes<Asteroid>());
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
  const common::Camera& camera = camera_->camera();
  const glm::mat4 proj = camera.GetProjectionMatrix();
  *planet_constant_->HostData<PlanetTrans>(frame) =
      {model, proj * camera.GetViewMatrix()};
  skybox_constant_->HostData<SkyboxTrans>(frame)->proj_view_model =
      proj * camera.GetSkyboxViewMatrix();
}

void PlanetApp::MainLoop() {
  const auto update_data = [this](int frame) { UpdateData(frame); };

  Recreate();
  while (!should_quit_ && mutable_window_context()->CheckEvents()) {
    timer_.Tick();

    const std::vector<RenderPass::RenderOp> render_ops{
        [this](const VkCommandBuffer& command_buffer) {
          planet_model_->Draw(command_buffer, current_frame_,
                              /*instance_count=*/1);
          asteroid_model_->Draw(command_buffer, current_frame_,
                                static_cast<uint32_t>(num_asteroids_.value()));
          skybox_model_->Draw(command_buffer, current_frame_,
                              /*instance_count=*/1);
        },
    };
    const auto draw_result = command_->Run(
        current_frame_, window_context().swapchain(), update_data,
        [this, &render_ops](const VkCommandBuffer& command_buffer,
                            uint32_t framebuffer_index) {
          render_pass().Run(command_buffer, framebuffer_index, render_ops);
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
  return AppMain<PlanetApp>(argc, argv, WindowContext::Config{});
}
