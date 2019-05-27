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

#include "absl/strings/str_format.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/time.h"
#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/context.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
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
struct LightTrans {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 proj_view;
  alignas(16) glm::vec4 direction_time;
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

class PlanetApp {
 public:
  PlanetApp() : context_{Context::GetContext()} {
    context_->Init("Planet");
  };
  void MainLoop();

 private:
  void Init();
  void GenAsteroidModels();
  void UpdateData(int frame);
  void Cleanup();

  bool should_quit_ = false;
  bool is_first_time = true;
  int current_frame_ = 0;
  common::Timer timer_;
  std::shared_ptr<Context> context_;
  common::Camera camera_;
  Command command_;
  Model planet_model_, asteroid_model_, skybox_model_;
  int num_asteroid_;
  PerInstanceBuffer per_asteroid_data_;
  PushConstant light_constant_, skybox_constant_;
};

} /* namespace */

void PlanetApp::Init() {
  if (is_first_time) {
    using KeyMap = common::Window::KeyMap;

    auto& window = context_->window();
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
    light_constant_.Init(sizeof(LightTrans), kNumFrameInFlight);
    skybox_constant_.Init(sizeof(SkyboxTrans), kNumFrameInFlight);

    is_first_time = false;
  }

  // model
  Model::TextureBindingMap planet_bindings;
  planet_bindings[Model::TextureType::kTypeDiffuse] = {
      /*binding_point=*/2, {{"external/resource/texture/planet.png"}},
  };
  planet_model_.Init(context_,
                     {{VK_SHADER_STAGE_VERTEX_BIT,
                       "jessie_steamer/shader/vulkan/planet.vert.spv"},
                      {VK_SHADER_STAGE_FRAGMENT_BIT,
                       "jessie_steamer/shader/vulkan/planet.frag.spv"}},
                     Model::SingleMeshResource{
                         "external/resource/model/sphere.obj",
                         /*obj_index_base=*/1, planet_bindings},
                     /*uniform_infos=*/absl::nullopt,
                     /*instancing_info=*/absl::nullopt,
                     absl::make_optional<Model::PushConstantInfos>(
                         {{VK_SHADER_STAGE_VERTEX_BIT
                               | VK_SHADER_STAGE_FRAGMENT_BIT,
                           {{&light_constant_, /*offset=*/0}}}}),
                     kNumFrameInFlight, /*is_opaque=*/true);

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
  asteroid_model_.Init(context_,
                       {{VK_SHADER_STAGE_VERTEX_BIT,
                         "jessie_steamer/shader/vulkan/asteroid.vert.spv"},
                        {VK_SHADER_STAGE_FRAGMENT_BIT,
                         "jessie_steamer/shader/vulkan/planet.frag.spv"}},
                       Model::MultiMeshResource{
                           "external/resource/model/rock/rock.obj",
                           "external/resource/model/rock",
                           {{Model::TextureType::kTypeDiffuse,
                             /*binding_point=*/2}},
                           /*extra_texture_map=*/absl::nullopt},
                       /*uniform_infos=*/absl::nullopt,
                       Model::InstancingInfo{
                           per_instance_attribs,
                           static_cast<uint32_t>(sizeof(Asteroid)),
                           &per_asteroid_data_},
                       absl::make_optional<Model::PushConstantInfos>(
                           {{VK_SHADER_STAGE_VERTEX_BIT
                                 | VK_SHADER_STAGE_FRAGMENT_BIT,
                             {{&light_constant_, /*offset=*/0}}}}),
                       kNumFrameInFlight, /*is_opaque=*/true);

  Model::TextureBindingMap skybox_bindings;
  skybox_bindings[Model::TextureType::kTypeCubemap] = {
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
  skybox_model_.Init(context_,
                     {{VK_SHADER_STAGE_VERTEX_BIT,
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
                     kNumFrameInFlight, /*is_opaque=*/true);

  // camera
  camera_.Calibrate(context_->window().screen_size(),
                    context_->window().cursor_pos());

  // command
  command_.Init(context_, kNumFrameInFlight);
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

  per_asteroid_data_.Init(context_, asteroids.data(),
                          sizeof(Asteroid) * asteroids.size());
}

void PlanetApp::UpdateData(int frame) {
  const float elapsed_time = timer_.time_from_launch();

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(5.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  glm::mat4 view = camera_.view();
  glm::mat4 proj = camera_.projection();

  glm::vec3 light_dir{glm::sin(elapsed_time * 0.6f), -0.3f,
                      glm::cos(elapsed_time * 0.6f)};
  *light_constant_.data<LightTrans>(frame) = {
      model,
      proj * view,
      glm::vec4{light_dir, elapsed_time},
  };

  *skybox_constant_.data<SkyboxTrans>(frame) = {proj, view};
}

void PlanetApp::MainLoop() {
  Init();
  const auto update_data = [this](int frame) {
    UpdateData(frame);
  };
  auto& window = context_->window();

  while (!should_quit_ && !window.ShouldQuit()) {
    auto draw_result = command_.Draw(
        current_frame_, update_data,
        [&](const VkCommandBuffer& command_buffer,
            const VkFramebuffer& framebuffer) {
      // start render pass
      std::array<VkClearValue, 2> clear_values{};
      clear_values[0].color.float32[0] = 0.0f;
      clear_values[0].color.float32[1] = 0.0f;
      clear_values[0].color.float32[2] = 0.0f;
      clear_values[0].color.float32[3] = 1.0f;
      clear_values[1].depthStencil = {1.0f, 0};  // initial depth set to 1.0

      VkRenderPassBeginInfo begin_info{
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          /*pNext=*/nullptr,
          *context_->render_pass(),
          framebuffer,
          /*renderArea=*/{
              /*offset=*/{0, 0},
              context_->swapchain().extent(),
          },
          CONTAINER_SIZE(clear_values),
          clear_values.data(),  // used for _OP_CLEAR
      };

      // record commends. options:
      //   - VK_SUBPASS_CONTENTS_INLINE: use primary command buffer
      //   - VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: use secondary
      vkCmdBeginRenderPass(command_buffer, &begin_info,
                           VK_SUBPASS_CONTENTS_INLINE);

      planet_model_.Draw(command_buffer, current_frame_, /*instance_count=*/1);
      asteroid_model_.Draw(command_buffer, current_frame_,
                           static_cast<uint32_t>(num_asteroid_));
      skybox_model_.Draw(command_buffer, current_frame_, /*instance_count=*/1);

      vkCmdEndRenderPass(command_buffer);
    });

    if (draw_result != VK_SUCCESS) {
      context_->WaitIdle();
      Cleanup();
      context_->Recreate();
      Init();
    }

    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
    window.PollEvents();
    camera_.set_activate(true);  // not activated until first frame is displayed
    const auto frame_rate = timer_.frame_rate();
    if (frame_rate.has_value()) {
      std::cout << absl::StrFormat("Frame per second: %d", frame_rate.value())
                << std::endl;
    }
  }
  context_->WaitIdle();  // wait for all async operations finish
}

void PlanetApp::Cleanup() {
  command_.Cleanup();
}

} /* namespace planet */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, const char* argv[]) {
#ifdef DEBUG
  INSERT_DEBUG_REQUIREMENT(/*overwrite=*/true);
  jessie_steamer::application::vulkan::planet::PlanetApp app{};
  app.MainLoop();
#else
  try {
    jessie_steamer::application::vulkan::planet::PlanetApp app{};
    app.MainLoop();
  } catch (const std::exception& e) {
    std::cerr << "Error: /n/t" << e.what() << std::endl;
    return EXIT_FAILURE;
  }
#endif /* DEBUG */
  return EXIT_SUCCESS;
}
