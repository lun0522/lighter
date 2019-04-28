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
  PlanetApp()
    : context_{Context::CreateContext()},
      camera_{/*position=*/glm::vec3{0.0f, 0.0f, 30.0f}} {
    context_->Init("Planet");
  };
  void MainLoop();

 private:
  void Init();
  size_t GenAsteroidModels();
  void UpdateData(size_t frame_index);
  void Cleanup();

  bool should_quit_ = false;
  bool is_first_time = true;
  size_t current_frame_ = 0;
  util::TimePoint last_time_;
  std::shared_ptr<Context> context_;
  common::Camera camera_;
  Command command_;
  Model planet_model_, asteroid_model_, skybox_model_;
  UniformBuffer trans_uniform_, asteroid_uniform_, light_uniform_;
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
    window.RegisterCursorMoveCallback([this](double x_pos, double y_pos) {
      camera_.ProcessCursorMove(x_pos, y_pos);
    });
    window.RegisterScrollCallback([this](double x_pos, double y_pos) {
      camera_.ProcessScroll(y_pos, 1.0f, 60.0f);
    });
    window.RegisterKeyCallback(KeyMap::kUp, [this]() {
      camera_.ProcessKey(KeyMap::kUp,
                         util::TimeInterval(last_time_, util::Now()));
    });
    window.RegisterKeyCallback(KeyMap::kDown, [this]() {
      camera_.ProcessKey(KeyMap::kDown,
                         util::TimeInterval(last_time_, util::Now()));
    });
    window.RegisterKeyCallback(KeyMap::kLeft, [this]() {
      camera_.ProcessKey(KeyMap::kLeft,
                         util::TimeInterval(last_time_, util::Now()));
    });
    window.RegisterKeyCallback(KeyMap::kRight, [this]() {
      camera_.ProcessKey(KeyMap::kRight,
                         util::TimeInterval(last_time_, util::Now()));
    });

    // uniform buffer
    trans_uniform_.Init(context_, /*is_per_instance=*/false,
                        UniformBuffer::Info{
                            sizeof(Transformation),
                            context_->swapchain().size()});
    light_uniform_.Init(context_, /*is_per_instance=*/false,
                        UniformBuffer::Info{
                            sizeof(Light),
                            context_->swapchain().size()});

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
  auto trans_infos = absl::make_optional<Model::UniformInfos>(
      {{trans_uniform_, trans_desc_info}});
  auto light_infos = absl::make_optional<Model::UniformInfos>(
      {{trans_uniform_, trans_desc_info},
       {light_uniform_, light_desc_info}});

  Model::TextureBindingMap planet_bindings;
  planet_bindings[Model::TextureType::kTypeDiffuse] = {
      /*binding_point=*/2, {{"jessie_steamer/resource/texture/planet.png"}},
  };
  planet_model_.Init(context_,
                     {{VK_SHADER_STAGE_VERTEX_BIT,
                       "jessie_steamer/shader/compiled/planet.vert.spv"},
                      {VK_SHADER_STAGE_FRAGMENT_BIT,
                       "jessie_steamer/shader/compiled/planet.frag.spv"}},
                     Model::SingleMeshResource{
                         "jessie_steamer/resource/model/sphere.obj",
                         /*obj_index_base=*/1, planet_bindings},
                     light_infos, /*instancing_info=*/absl::nullopt,
                     /*push_constants=*/nullptr, kNumFrameInFlight,
                     /*is_opaque=*/true);

  size_t num_asteroid = GenAsteroidModels();
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
                         "jessie_steamer/shader/compiled/asteroid.vert.spv"},
                        {VK_SHADER_STAGE_FRAGMENT_BIT,
                         "jessie_steamer/shader/compiled/planet.frag.spv"}},
                       Model::MultiMeshResource{
                           "jessie_steamer/resource/model/rock/rock.obj",
                           "jessie_steamer/resource/model/rock",
                           {{Model::TextureType::kTypeDiffuse,
                             /*binding_point=*/2}}},
                       light_infos,
                       Model::InstancingInfo{
                           per_instance_attribs,
                           static_cast<uint32_t>(sizeof(Asteroid)),
                           &asteroid_uniform_},
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
                       "jessie_steamer/shader/compiled/skybox.vert.spv"},
                      {VK_SHADER_STAGE_FRAGMENT_BIT,
                       "jessie_steamer/shader/compiled/skybox.frag.spv"}},
                     Model::SingleMeshResource{
                         "jessie_steamer/resource/model/skybox.obj",
                         /*obj_index_base=*/1, skybox_bindings},
                     trans_infos, /*instancing_info=*/absl::nullopt,
                     /*push_constants=*/nullptr, kNumFrameInFlight,
                     /*is_opaque=*/true);

  // time
  last_time_ = util::Now();

  // camera
  camera_.Init(context_->window().screen_size(),
               context_->window().cursor_pos());

  // command
  command_.Init(context_, kNumFrameInFlight,
                [&](const VkCommandBuffer& command_buffer, size_t image_index) {
    // start render pass
    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color.float32[0] = 0.0f;
    clear_values[0].color.float32[1] = 0.0f;
    clear_values[0].color.float32[2] = 0.0f;
    clear_values[0].color.float32[3] = 1.0f;
    clear_values[1].depthStencil = {1.0f, 0};  // initial depth value set to 1.0

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
                         static_cast<uint32_t>(num_asteroid));
    skybox_model_.Draw(command_buffer, image_index, /*instance_count=*/1);

    vkCmdEndRenderPass(command_buffer);
  });
}

size_t PlanetApp::GenAsteroidModels() {
  const std::array<int, kNumAsteroidRing> num_asteroid = {600, 800, 1000};
  const std::array<float, kNumAsteroidRing> radii = {5.0f, 10.0f, 15.0f};

  std::random_device device;
  std::mt19937 rand_gen{device()};
  std::uniform_real_distribution<float> axis_gen{0.0f, 1.0f};
  std::uniform_real_distribution<float> angle_gen{0.0f, 360.0f};
  std::uniform_real_distribution<float> radius_gen{-1.5f, 1.5f};
  std::uniform_real_distribution<float> scale_gen{1.0f, 3.0f};

  const auto total_num_asteroid = static_cast<size_t>(std::accumulate(
      num_asteroid.begin(), num_asteroid.end(), 0));
  asteroid_uniform_.Init(context_, /*is_per_instance=*/true,
                         UniformBuffer::Info{
                             total_num_asteroid * sizeof(Asteroid),
                             /*num_chunk=*/1});

  auto* asteroids = asteroid_uniform_.data<Asteroid>(0);
  for (size_t ring = 0; ring < kNumAsteroidRing; ++ring) {
    for (size_t i = 0; i < num_asteroid[ring]; ++i) {
      glm::mat4 model{1.0f};
      model = glm::rotate(model, glm::radians(angle_gen(rand_gen)),
                          glm::vec3{axis_gen(rand_gen), axis_gen(rand_gen),
                                    axis_gen(rand_gen)});
      model = glm::scale(model, glm::vec3{scale_gen(rand_gen) * 0.02f});
      *asteroids = {
          /*theta=*/glm::radians(angle_gen(rand_gen)),
          /*radius=*/radii[ring] + radius_gen(rand_gen),
          std::move(model),
      };
      ++asteroids;
    }
  }
  asteroid_uniform_.UpdateData(0);

  return total_num_asteroid;
}

void PlanetApp::UpdateData(size_t frame_index) {
  glm::mat4 model{1.0f};
  static auto start_time = util::Now();
  auto elapsed_time = util::TimeInterval(start_time, util::Now());
  model = glm::rotate(model, elapsed_time * glm::radians(5.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});

  auto* trans = trans_uniform_.data<Transformation>(frame_index);
  *trans = {model, camera_.view_matrix(), camera_.proj_matrix()};
  // no need to flip Y-axis as OpenGL
  trans->proj[1][1] *= -1;

  glm::vec3 light_dir{glm::sin(elapsed_time * 0.6f), -0.7f,
                      glm::cos(elapsed_time * 0.6f)};
  light_uniform_.data<Light>(frame_index)->direction_time =
      glm::vec4{light_dir, elapsed_time};
}

void PlanetApp::MainLoop() {
  Init();
  const auto update_data = [this](size_t frame_index) {
    UpdateData(frame_index);
    trans_uniform_.UpdateData(frame_index);
    light_uniform_.UpdateData(frame_index);
  };
  auto& window = context_->window();
  while (!should_quit_ && !window.ShouldQuit()) {
    window.PollEvents();
    last_time_ = util::Now();

    if (command_.DrawFrame(current_frame_, update_data) != VK_SUCCESS) {
      context_->WaitIdle();
      Cleanup();
      context_->Recreate();
      Init();
    }
    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
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
