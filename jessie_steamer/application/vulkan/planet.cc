//
//  planet.cc
//
//  Created by Pujun Lun on 4/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/time.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/window_context.h"
#include "third_party/glm/glm.hpp"
// different from OpenGL, where depth values are in range [-1.0, 1.0]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "third_party/glm/gtc/matrix_transform.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace planet {
namespace {

using namespace wrapper::vulkan;

constexpr int kNumFrameInFlight = 2;
constexpr int kNumAsteroidRing = 3;

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct Light {
  alignas(16) glm::vec4 direction_time;
};

struct PlanetTrans {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 proj_view;
};

struct SkyboxTrans {
  alignas(16) glm::mat4 proj;
  alignas(16) glm::mat4 view;
};

struct Asteroid {
  float theta;
  float radius;
  glm::mat4 model;
};

class PlanetApp : public Application {
 public:
  PlanetApp() : command_{context()}, planet_model_{context()},
                asteroid_model_{context()}, skybox_model_{context()},
                per_asteroid_data_{context()}, light_uniform_{context()} {}
  void MainLoop() override;

 private:
  void Init();
  void GenAsteroidModels();
  void UpdateData(int frame);

  bool should_quit_ = false;
  bool is_first_time = true;
  int current_frame_ = 0;
  common::Timer timer_;
  common::Camera camera_;
  PerFrameCommand command_;
  Model planet_model_, asteroid_model_, skybox_model_;
  int num_asteroid_;
  PerInstanceBuffer per_asteroid_data_;
  UniformBuffer light_uniform_;
  PushConstant planet_constant_, skybox_constant_;
  std::unique_ptr<DepthStencilImage> depth_stencil_;
  std::unique_ptr<RenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
};

} /* namespace */

void PlanetApp::Init() {
  // window
  window_context_.Init("Planet");

  // depth stencil
  auto frame_size = window_context_.frame_size();
  depth_stencil_ = absl::make_unique<DepthStencilImage>(context(), frame_size);

  if (is_first_time) {
    is_first_time = false;

    using KeyMap = common::Window::KeyMap;

    auto& window = window_context_.window();
    window.SetCursorHidden(true);
    window.RegisterKeyCallback(KeyMap::kEscape,
                               [this]() { should_quit_ = true; });

    // camera
    common::Camera::Config config;
    config.pos = glm::vec3{1.6f, -5.1f, -5.9f};
    config.look_at = glm::vec3{-2.4f, -0.8f, 0.0f};
    camera_.Init(config);

    window.RegisterCursorMoveCallback([this](double x_pos, double y_pos) {
      camera_.ProcessCursorMove(x_pos, y_pos);
    });
    window.RegisterScrollCallback([this](double x_pos, double y_pos) {
      camera_.ProcessScroll(y_pos, 1.0f, 60.0f);
    });
    window.RegisterKeyCallback(KeyMap::kUp, [this]() {
      camera_.ProcessKey(KeyMap::kUp, timer_.time_from_last_frame());
    });
    window.RegisterKeyCallback(KeyMap::kDown, [this]() {
      camera_.ProcessKey(KeyMap::kDown, timer_.time_from_last_frame());
    });
    window.RegisterKeyCallback(KeyMap::kLeft, [this]() {
      camera_.ProcessKey(KeyMap::kLeft, timer_.time_from_last_frame());
    });
    window.RegisterKeyCallback(KeyMap::kRight, [this]() {
      camera_.ProcessKey(KeyMap::kRight, timer_.time_from_last_frame());
    });

    // push constants
    light_uniform_.Init(sizeof(Light), kNumFrameInFlight);
    planet_constant_.Init(sizeof(PlanetTrans), kNumFrameInFlight);
    skybox_constant_.Init(sizeof(SkyboxTrans), kNumFrameInFlight);

    // render pass builder
    render_pass_builder_ = RenderPassBuilder::SimpleRenderPassBuilder(
        context(), *depth_stencil_, window_context_.swapchain().num_image(),
        /*get_swapchain_image=*/[this](int index) -> const Image& {
          return window_context_.swapchain().image(index);
        });
  }

  // render pass
  (*render_pass_builder_)
      .set_framebuffer_size(frame_size)
      .update_attachment(
          /*index=*/1, [this](int index) -> const Image& {
            return *depth_stencil_;
          });
  render_pass_ = render_pass_builder_->Build();

  // model
  Descriptor::Info light_desc_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
      /*bindings=*/{{
          Descriptor::ResourceType::kUniformBuffer,
          /*binding_point=*/1,
          /*array_length=*/1,
      }},
  };
  Model::TextureBindingMap planet_bindings;
  planet_bindings[Model::ResourceType::kTextureDiffuse] = {
      /*binding_point=*/2, {{"external/resource/texture/planet.png"}},
  };
  planet_model_.Init({{VK_SHADER_STAGE_VERTEX_BIT,
                       "jessie_steamer/shader/vulkan/planet.vert.spv"},
                      {VK_SHADER_STAGE_FRAGMENT_BIT,
                       "jessie_steamer/shader/vulkan/planet.frag.spv"}},
                     Model::SingleMeshResource{
                         "external/resource/model/sphere.obj",
                         /*obj_index_base=*/1, planet_bindings},
                     absl::make_optional<Model::UniformInfos>(
                         {{light_uniform_, light_desc_info}}),
                     /*instancing_info=*/absl::nullopt,
                     absl::make_optional<Model::PushConstantInfos>(
                         {{VK_SHADER_STAGE_VERTEX_BIT,
                           {{&planet_constant_, /*offset=*/0}}}}),
                     {**render_pass_, /*subpass=*/0},
                     frame_size, kNumFrameInFlight, /*is_opaque=*/true);

  GenAsteroidModels();
  std::vector<Model::VertexAttribute> per_instance_attribs{
      {/*location=*/3, offsetof(Asteroid, theta), VK_FORMAT_R32_SFLOAT},
      {/*location=*/4, offsetof(Asteroid, radius), VK_FORMAT_R32_SFLOAT},
  };
  per_instance_attribs.reserve(6);
  int attrib_offset = offsetof(Asteroid, model);
  for (uint32_t location = 5; location <= 8; ++location) {
    per_instance_attribs.emplace_back(Model::VertexAttribute{
        location, static_cast<uint32_t>(attrib_offset),
        VK_FORMAT_R32G32B32A32_SFLOAT,
    });
    attrib_offset += sizeof(glm::vec4);
  }
  light_desc_info.shader_stage |= VK_SHADER_STAGE_VERTEX_BIT;
  asteroid_model_.Init({{VK_SHADER_STAGE_VERTEX_BIT,
                         "jessie_steamer/shader/vulkan/asteroid.vert.spv"},
                        {VK_SHADER_STAGE_FRAGMENT_BIT,
                         "jessie_steamer/shader/vulkan/planet.frag.spv"}},
                       Model::MultiMeshResource{
                           "external/resource/model/rock/rock.obj",
                           "external/resource/model/rock",
                           {{Model::ResourceType::kTextureDiffuse,
                             /*binding_point=*/2}},
                           /*extra_texture_map=*/absl::nullopt},
                       absl::make_optional<Model::UniformInfos>(
                           {{light_uniform_, light_desc_info}}),
                       Model::InstancingInfo{
                           per_instance_attribs,
                           static_cast<uint32_t>(sizeof(Asteroid)),
                           &per_asteroid_data_},
                       absl::make_optional<Model::PushConstantInfos>(
                           {{VK_SHADER_STAGE_VERTEX_BIT,
                             {{&planet_constant_, /*offset=*/0}}}}),
                       {**render_pass_, /*subpass=*/0},
                       frame_size, kNumFrameInFlight, /*is_opaque=*/true);

  Model::TextureBindingMap skybox_bindings;
  skybox_bindings[Model::ResourceType::kTextureCubemap] = {
      /*binding_point=*/1, {
          TextureImage::CubemapPath{
              /*directory=*/"external/resource/texture/universe",
              /*files=*/{
                  "PositiveX.jpg",
                  "NegativeX.jpg",
                  "PositiveY.jpg",
                  "NegativeY.jpg",
                  "PositiveZ.jpg",
                  "NegativeZ.jpg",
              },
          },
      },
  };
  skybox_model_.Init({{VK_SHADER_STAGE_VERTEX_BIT,
                       "jessie_steamer/shader/vulkan/skybox.vert.spv"},
                      {VK_SHADER_STAGE_FRAGMENT_BIT,
                       "jessie_steamer/shader/vulkan/skybox.frag.spv"}},
                     Model::SingleMeshResource{
                         "external/resource/model/skybox.obj",
                         /*obj_index_base=*/1, skybox_bindings},
                     /*uniform_infos=*/absl::nullopt,
                     /*instancing_info=*/absl::nullopt,
                     absl::make_optional<Model::PushConstantInfos>(
                         {{VK_SHADER_STAGE_VERTEX_BIT,
                           {{&skybox_constant_, /*offset=*/0}}}}),
                     {**render_pass_, /*subpass=*/0},
                     frame_size, kNumFrameInFlight, /*is_opaque=*/true);

  // camera
  camera_.Calibrate(window_context_.window().GetScreenSize(),
                    window_context_.window().GetCursorPos());

  // command
  command_.Init(kNumFrameInFlight, &window_context_.queues());
}

void PlanetApp::GenAsteroidModels() {
  const std::array<int, kNumAsteroidRing> num_asteroid = {300, 500, 700};
  const std::array<float, kNumAsteroidRing> radii = {6.0f, 12.0f,  18.0f};

  std::random_device device;
  std::mt19937 rand_gen{device()};
  std::uniform_real_distribution<float> axis_gen{0.0f, 1.0f};
  std::uniform_real_distribution<float> angle_gen{0.0f, 360.0f};
  std::uniform_real_distribution<float> radius_gen{-1.5f, 1.5f};
  std::uniform_real_distribution<float> scale_gen{1.0f, 3.0f};

  num_asteroid_ = static_cast<int>(std::accumulate(
      num_asteroid.begin(), num_asteroid.end(), 0));
  std::vector<Asteroid> asteroids;
  asteroids.reserve(num_asteroid_);

  for (int ring = 0; ring < kNumAsteroidRing; ++ring) {
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

  per_asteroid_data_.Init(asteroids.data(),
                          sizeof(Asteroid) * asteroids.size());
}

void PlanetApp::UpdateData(int frame) {
  const float elapsed_time = timer_.time_from_launch();

  glm::vec3 light_dir{glm::sin(elapsed_time * 0.6f), -0.3f,
                      glm::cos(elapsed_time * 0.6f)};
  *light_uniform_.data<Light>(frame) = {glm::vec4{light_dir, elapsed_time}};
  light_uniform_.Flush(frame);

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(5.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  glm::mat4 view = camera_.view();
  glm::mat4 proj = camera_.projection();
  *planet_constant_.data<PlanetTrans>(frame) = {model, proj * view};
  *skybox_constant_.data<SkyboxTrans>(frame) = {proj, view};
}

void PlanetApp::MainLoop() {
  Init();
  const auto update_data = [this](int frame) {
    UpdateData(frame);
  };
  while (!should_quit_ && !window_context_.ShouldQuit()) {
    auto draw_result = command_.Run(
        current_frame_, *window_context_.swapchain(), update_data,
        [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
          render_pass_->Run(
              command_buffer, framebuffer_index,
              [this, &command_buffer]() {
                planet_model_.Draw(command_buffer, current_frame_,
                                   /*instance_count=*/1);
                asteroid_model_.Draw(command_buffer, current_frame_,
                                     static_cast<uint32_t>(num_asteroid_));
                skybox_model_.Draw(command_buffer, current_frame_,
                                   /*instance_count=*/1);
              });
        });

    if (draw_result != VK_SUCCESS) {
      window_context_.Cleanup();
      command_.Cleanup();
      Init();
    }

    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
    window_context_.PollEvents();
    camera_.set_activate(true);  // not activated until first frame is displayed
    const auto frame_rate = timer_.frame_rate();
    if (frame_rate.has_value()) {
      std::cout << absl::StrFormat("Frame per second: %d", frame_rate.value())
                << std::endl;
    }
  }
  window_context_.WaitIdle();  // wait for all async operations finish
}

} /* namespace planet */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, const char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<planet::PlanetApp>();
}
