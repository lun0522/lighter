//
//  planet.cc
//
//  Created by Pujun Lun on 4/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <cstddef>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include "lighter/example/util.h"
#include "lighter/renderer/extension/model.h"

namespace lighter::example {
namespace {

using renderer::extension::Model;
using renderer::extension::ModelBuilder;
using renderer::ir::AccessLocation;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::DataFormat;
using renderer::ir::ImageHandle;
using renderer::ir::MultisamplingMode;
using renderer::ir::PassBuilder;
using renderer::ir::PassContext;
using renderer::ir::Renderer;
using renderer::ir::RenderGraph;
using renderer::ir::VertexBufferView;

constexpr int kWindowIndex = 0;
constexpr int kNumAsteroidRings = 3;
constexpr int kNumFramesInFlight = 2;
constexpr int kObjFileIndexBase = 1;

// Consistent with vertex input attributes defined in shaders.
struct Asteroid {
  static std::vector<VertexBufferView::Attribute> GetAttributes() {
    std::vector<VertexBufferView::Attribute> attributes{
        {/*location=*/0, DataFormat::kSFloat32, offsetof(Asteroid, theta)},
        {/*location=*/1, DataFormat::kSFloat32, offsetof(Asteroid, radius)},
    };
    for (int col = 0; col < 4; ++col) {
      attributes.push_back({
          /*location=*/2 + col,
          DataFormat::kSFloat32Vec4,
          offsetof(Asteroid, model) + sizeof(glm::vec4) * col,
      });
    }
    return attributes;
  }

  float theta;
  float radius;
  glm::mat4 model;
};

// Consistent with uniform blocks defined in shaders.
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

class PlanetExample {
 public:
  PlanetExample(const glm::ivec2& screen_size,
                MultisamplingMode multisampling_mode)
      : multisampling_mode_{multisampling_mode},
        window_{"Planet", screen_size} {
    using common::file::GetResourcePath;
    using TextureType = ModelBuilder::TextureType;

    renderer_ = CreateRenderer("Planet Example", {&window_});

    const float aspect_ratio = window_.original_aspect_ratio();
    common::Camera::Config camera_config;
    camera_config.position = glm::vec3{1.6f, -5.1f, -5.9f};
    camera_config.look_at = glm::vec3{-2.4f, -0.8f, 0.0f};
    camera_ = common::UserControlledPerspectiveCamera::Create(
        /*control_config=*/{}, camera_config,
        {/*field_of_view_y=*/45.0f, aspect_ratio});
    RegisterCameraControls(window_, timer_, *camera_, &should_quit_);

    const std::array light_usages{
        BufferUsage::GetUniformBufferUsage(AccessLocation::kVertexShader),
        BufferUsage::GetUniformBufferUsage(AccessLocation::kFragmentShader),
    };
    for (auto& buffer : light_uniforms_) {
      buffer = renderer_->CreateBuffer(Buffer::UpdateRate::kHigh, sizeof(Light),
                                       light_usages);
    }

    planet_model_ =
        ModelBuilder{*renderer_, "Planet", aspect_ratio,
                     ModelBuilder::SingleMeshResource{
                         GetResourcePath("model/sphere.obj"),
                         kObjFileIndexBase,
                         /*tex_source_map=*/
                         {{
                             TextureType::kDiffuse,
                             {GetResourcePath("texture/planet.png")},
                         }},
                     }}
            .AddTextureBindingPoint(TextureType::kDiffuse, /*binding=*/2)
            .SetShader(renderer::ir::shader_stage::VERTEX,
                       GetShaderBinaryPath("planet/planet.vert"))
            .SetShader(renderer::ir::shader_stage::FRAGMENT,
                       GetShaderBinaryPath("planet/planet.frag"))
            .Build();

    GenerateAsteroidModels();
    asteroid_model_ =
        ModelBuilder{*renderer_, "Asteroid", aspect_ratio,
                     ModelBuilder::MultiMeshResource{
                         /*model_path=*/GetResourcePath("model/rock/rock.obj"),
                         /*texture_dir=*/
                         GetResourcePath("model/rock/rock.obj",
                                         /*want_directory_path=*/true),
                     }}
            .AddTextureBindingPoint(TextureType::kDiffuse, /*binding=*/2)
            .AddPerInstanceBuffer(Asteroid::GetAttributes(), sizeof(Asteroid),
                                  per_asteroid_data_.get())
            .SetShader(renderer::ir::shader_stage::VERTEX,
                       GetShaderBinaryPath("planet/asteroid.vert"))
            .SetShader(renderer::ir::shader_stage::FRAGMENT,
                       GetShaderBinaryPath("planet/planet.frag"))
            .Build();

    const ModelBuilder::CubemapPath skybox_path{
        .directory = GetResourcePath("texture/universe/PositiveX.jpg",
                                     /*want_directory_path=*/true),
        .files = {"PositiveX.jpg", "NegativeX.jpg", "PositiveY.jpg",
                  "NegativeY.jpg", "PositiveZ.jpg", "NegativeZ.jpg"},
    };

    skybox_model_ =
        ModelBuilder{
            *renderer_, "Skybox", aspect_ratio,
            ModelBuilder::SingleMeshResource{
                GetResourcePath("model/skybox.obj"),
                kObjFileIndexBase,
                /*tex_source_map=*/{{TextureType::kCubemap, {skybox_path}}},
            }}
            .AddTextureBindingPoint(TextureType::kCubemap, /*binding=*/1)
            .SetShader(renderer::ir::shader_stage::VERTEX,
                       GetShaderBinaryPath("shared/skybox.vert"))
            .SetShader(renderer::ir::shader_stage::FRAGMENT,
                       GetShaderBinaryPath("shared/skybox.frag"))
            .Build();
  }

  // This class is neither copyable nor movable.
  PlanetExample(const PlanetExample&) = delete;
  PlanetExample& operator=(const PlanetExample&) = delete;

  void MainLoop() {
    camera_->SetCursorPos(window_.GetCursorPos());
    RenderGraph graph;

    while (!should_quit_ && !window_.ShouldQuit()) {
      window_.ProcessUserInputs();
      timer_.Tick();

      const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();
      const glm::vec3 light_dir{glm::sin(elapsed_time * 0.6f), -0.3f,
                                glm::cos(elapsed_time * 0.6f)};
      const Light light{glm::vec4{light_dir, elapsed_time}};

      glm::mat4 model{1.0f};
      model = glm::rotate(model, elapsed_time * glm::radians(5.0f),
                          glm::vec3{0.0f, 1.0f, 0.0f});
      const common::Camera& cam = camera_->camera();
      const glm::mat4 proj = cam.GetProjectionMatrix();
      const PlanetTrans planet_trans{model, proj * cam.GetViewMatrix()};
      const SkyboxTrans skybox_trans{proj * cam.GetSkyboxViewMatrix()};

      graph.Reset();
      const ImageHandle backbuffer = graph.ImportSwapchainImage(kWindowIndex);
      graph.AddGraphicsPass(
          "PlanetScene",
          [&](PassBuilder& builder) {
            SetupMainPassAttachments(builder, backbuffer, multisampling_mode_,
                                     /*use_depth=*/true);
          },
          [&](PassContext& ctx) {
            const glm::ivec2 frame_size = ctx.GetImageExtent(backbuffer);
            const Buffer& light_uniform = *light_uniforms_[frame_slot_];
            light_uniform.CopyToDevice(std::array{
                Buffer::CopyInfo{&light, sizeof(Light), /*offset=*/0}});

            planet_model_->BindPipeline(ctx, frame_size);
            ctx.BindBuffer(/*binding=*/1, light_uniform);
            ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                              sizeof(PlanetTrans), &planet_trans);
            planet_model_->Draw(ctx);

            asteroid_model_->BindPipeline(ctx, frame_size);
            ctx.BindBuffer(/*binding=*/1, light_uniform);
            ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                              sizeof(PlanetTrans), &planet_trans);
            asteroid_model_->Draw(ctx, static_cast<uint32_t>(num_asteroids_));

            skybox_model_->BindPipeline(ctx, frame_size);
            ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                              sizeof(SkyboxTrans), &skybox_trans);
            skybox_model_->Draw(ctx);
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
  void GenerateAsteroidModels() {
    const std::array<int, kNumAsteroidRings> num_asteroid{300, 500, 700};
    const std::array<float, kNumAsteroidRings> radii{6.0f, 12.0f, 18.0f};

    std::random_device device;
    std::mt19937 rand_gen{device()};
    std::uniform_real_distribution<float> axis_gen{0.0f, 1.0f};
    std::uniform_real_distribution<float> angle_gen{0.0f, 360.0f};
    std::uniform_real_distribution<float> radius_gen{-1.5f, 1.5f};
    std::uniform_real_distribution<float> scale_gen{1.0f, 3.0f};

    num_asteroids_ =
        std::accumulate(num_asteroid.begin(), num_asteroid.end(), 0);
    std::vector<Asteroid> asteroids;
    asteroids.reserve(static_cast<size_t>(num_asteroids_));

    for (int ring = 0; ring < kNumAsteroidRings; ++ring) {
      for (int i = 0; i < num_asteroid[ring]; ++i) {
        glm::mat4 model{1.0f};
        model = glm::rotate(model, glm::radians(angle_gen(rand_gen)),
                            glm::vec3{axis_gen(rand_gen), axis_gen(rand_gen),
                                      axis_gen(rand_gen)});
        model = glm::scale(model, glm::vec3{scale_gen(rand_gen) * 0.02f});

        asteroids.push_back(Asteroid{
            .theta = glm::radians(angle_gen(rand_gen)),
            .radius = radii[ring] + radius_gen(rand_gen),
            .model = model,
        });
      }
    }

    const size_t data_size =
        common::util::GetTotalDataSize(std::span<const Asteroid>{asteroids});
    per_asteroid_data_ =
        renderer_->CreateBuffer(Buffer::UpdateRate::kLow, data_size,
                                std::array{BufferUsage::GetVertexBufferUsage(
                                    BufferUsage::UsageType::kVertexOnly)});
    per_asteroid_data_->CopyToDevice(std::array{
        Buffer::CopyInfo{asteroids.data(), data_size, /*offset=*/0}});
  }

  const MultisamplingMode multisampling_mode_;
  bool should_quit_ = false;
  int frame_slot_ = 0;
  int num_asteroids_ = 0;
  common::FrameTimer timer_;
  common::Window window_;
  std::unique_ptr<common::UserControlledPerspectiveCamera> camera_;
  std::unique_ptr<Renderer> renderer_;
  std::array<std::unique_ptr<Buffer>, kNumFramesInFlight> light_uniforms_;
  std::unique_ptr<Buffer> per_asteroid_data_;
  std::unique_ptr<Model> planet_model_;
  std::unique_ptr<Model> asteroid_model_;
  std::unique_ptr<Model> skybox_model_;
};

}  // namespace
}  // namespace lighter::example

int main(int argc, char* argv[]) {
  return lighter::example::ExampleMain<lighter::example::PlanetExample>(
      argc, argv, glm::ivec2{800, 600},
      lighter::renderer::ir::MultisamplingMode::kDecent);
}
