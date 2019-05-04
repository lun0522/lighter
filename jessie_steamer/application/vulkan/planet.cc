//
//  planet.cc
//
//  Created by Pujun Lun on 4/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
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

namespace util = common::util;
using namespace wrapper::vulkan;

constexpr size_t kNumFrameInFlight = 2;
constexpr size_t kNumAsteroidRing = 3;

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct Transformation {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 proj_view;
};

struct SkyboxTrans {
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

struct Light {
  alignas(16) glm::vec4 direction_time;
};

struct Asteroid {
  float theta;
  float radius;
  glm::mat4 model;
};

class PlanetApp {
 public:
  PlanetApp() : context_{Context::CreateContext()} {
    context_->Init("Planet");
  };
  void MainLoop();

 private:
  void Init();
  void GenAsteroidModels();
  void UpdateData(size_t frame_index);
  void Cleanup();

  bool should_quit_ = false;
  bool is_first_time = true;
  size_t current_frame_ = 0;
  util::Timer timer_;
  std::shared_ptr<Context> context_;
  common::Camera camera_;
  Command command_;
  Model planet_model_, asteroid_model_, skybox_model_;
  size_t num_asteroid_;
  PerInstanceBuffer per_asteroid_data_;
  UniformBuffer trans_uniform_, light_uniform_, skybox_uniform_;
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
    config.pos = glm::vec3{-1.0f, -4.5f, -5.5f};
    config.look_at = glm::vec3{-2.0f, -1.2f, 0.0f};
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

    // uniform buffer
    trans_uniform_.Init(context_, UniformBuffer::Info{
        sizeof(Transformation),
        context_->swapchain().size()});
    light_uniform_.Init(context_, UniformBuffer::Info{
        sizeof(Light),
        context_->swapchain().size()});
    skybox_uniform_.Init(context_, UniformBuffer::Info{
        sizeof(SkyboxTrans),
        context_->swapchain().size(),
    });

    is_first_time = false;
  }

  // model
  Descriptor::Info trans_desc_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      /*shader_stage=*/VK_SHADER_STAGE_VERTEX_BIT,
      /*bindings=*/{{
          Descriptor::TextureType::kTypeMaxEnum,
          /*binding_point=*/0,
          /*array_length=*/1,
      }},
  };
  Descriptor::Info light_desc_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      /*shader_stage=*/VK_SHADER_STAGE_VERTEX_BIT
                           | VK_SHADER_STAGE_FRAGMENT_BIT,
      /*bindings=*/{{
          Descriptor::TextureType::kTypeMaxEnum,
          /*binding_point=*/1,
          /*array_length=*/1,
      }},
  };
  auto trans_light_infos = absl::make_optional<Model::UniformInfos>(
      {{trans_uniform_, trans_desc_info},
       {light_uniform_, light_desc_info}});

  Model::TextureBindingMap planet_bindings;
  planet_bindings[Model::TextureType::kTypeDiffuse] = {
      /*binding_point=*/2, {{"jessie_steamer/resource/texture/planet.png"}},
  };
  planet_model_.Init(context_,
                     {{VK_SHADER_STAGE_VERTEX_BIT,
                       "jessie_steamer/shader/vulkan/planet.vert.spv"},
                      {VK_SHADER_STAGE_FRAGMENT_BIT,
                       "jessie_steamer/shader/vulkan/planet.frag.spv"}},
                     Model::SingleMeshResource{
                         "jessie_steamer/resource/model/sphere.obj",
                         /*obj_index_base=*/1, planet_bindings},
                     trans_light_infos, /*instancing_info=*/absl::nullopt,
                     /*push_constants=*/nullptr, kNumFrameInFlight,
                     /*is_opaque=*/true);

  GenAsteroidModels();
  std::vector<Model::VertexAttribute> per_instance_attribs{
      {/*location=*/3, offsetof(Asteroid, theta), VK_FORMAT_R32_SFLOAT},
      {/*location=*/4, offsetof(Asteroid, radius), VK_FORMAT_R32_SFLOAT},
  };
  per_instance_attribs.reserve(6);
  size_t attrib_offset = offsetof(Asteroid, model);
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
                           "jessie_steamer/resource/model/rock/rock.obj",
                           "jessie_steamer/resource/model/rock",
                           {{Model::TextureType::kTypeDiffuse,
                             /*binding_point=*/2}}},
                       trans_light_infos,
                       Model::InstancingInfo{
                           per_instance_attribs,
                           static_cast<uint32_t>(sizeof(Asteroid)),
                           &per_asteroid_data_},
                       /*push_constants=*/nullptr, kNumFrameInFlight,
                       /*is_opaque=*/true);

  const std::string skybox_dir{"jessie_steamer/resource/texture/universe/"};
  Model::TextureBindingMap skybox_bindings;
  skybox_bindings[Model::TextureType::kTypeSpecular] = {
      /*binding_point=*/1, {{
          skybox_dir + "PositiveX.jpg",
          skybox_dir + "NegativeX.jpg",
          skybox_dir + "PositiveY.jpg",
          skybox_dir + "NegativeY.jpg",
          skybox_dir + "PositiveZ.jpg",
          skybox_dir + "NegativeZ.jpg",
      }},
  };
  skybox_model_.Init(context_,
                     {{VK_SHADER_STAGE_VERTEX_BIT,
                       "jessie_steamer/shader/vulkan/skybox.vert.spv"},
                      {VK_SHADER_STAGE_FRAGMENT_BIT,
                       "jessie_steamer/shader/vulkan/skybox.frag.spv"}},
                     Model::SingleMeshResource{
                         "jessie_steamer/resource/model/skybox.obj",
                         /*obj_index_base=*/1, skybox_bindings},
                     absl::make_optional<Model::UniformInfos>(
                         {{skybox_uniform_, trans_desc_info}}),
                     /*instancing_info=*/absl::nullopt,
                     /*push_constants=*/nullptr, kNumFrameInFlight,
                     /*is_opaque=*/true);

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

  num_asteroid_ = static_cast<size_t>(std::accumulate(
      num_asteroid.begin(), num_asteroid.end(), 0));
  std::vector<Asteroid> asteroids;
  asteroids.reserve(num_asteroid_);

  for (size_t ring = 0; ring < kNumAsteroidRing; ++ring) {
    for (size_t i = 0; i < num_asteroid[ring]; ++i) {
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

void PlanetApp::UpdateData(size_t frame_index) {
  const float elapsed_time = timer_.time_from_launch();

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(5.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  glm::mat4 view = camera_.view();
  glm::mat4 proj = camera_.projection();

  auto* trans = trans_uniform_.data<Transformation>(frame_index);
  trans->model = model;
  trans->proj_view = proj * view;

  glm::vec3 light_dir{glm::sin(elapsed_time * 0.6f), -0.3f,
                      glm::cos(elapsed_time * 0.6f)};
  light_uniform_.data<Light>(frame_index)->direction_time =
      glm::vec4{light_dir, elapsed_time};

  auto* skybox_trans = skybox_uniform_.data<SkyboxTrans>(frame_index);
  skybox_trans->proj = proj;
  skybox_trans->view = view;
}

void PlanetApp::MainLoop() {
  Init();
  const auto update_data = [this](size_t frame_index) {
    UpdateData(frame_index);
    trans_uniform_.UpdateData(frame_index);
    light_uniform_.UpdateData(frame_index);
    skybox_uniform_.UpdateData(frame_index);
  };
  auto& window = context_->window();

  while (!should_quit_ && !window.ShouldQuit()) {
    auto draw_result = command_.DrawFrame(
        current_frame_, update_data,
        [&](const VkCommandBuffer& command_buffer, size_t image_index) {
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
          context_->render_pass().framebuffer(image_index),
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

      planet_model_.Draw(command_buffer, image_index, /*instance_count=*/1);
      asteroid_model_.Draw(command_buffer, image_index,
                           static_cast<uint32_t>(num_asteroid_));
      skybox_model_.Draw(command_buffer, image_index, /*instance_count=*/1);

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
    camera_.Activate();  // not activated until first frame is displayed
    const auto frame_rate = timer_.frame_rate();
    if (frame_rate.has_value()) {
      std::cout << "Frame per second: " << frame_rate.value() << std::endl;
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
