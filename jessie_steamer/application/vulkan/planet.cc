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
#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/common/time.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/render_pass_util.h"
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

using common::file::GetResourcePath;
using common::file::GetShaderPath;
using std::vector;

constexpr int kNumFrameInFlight = 2;
constexpr int kNumAsteroidRing = 3;

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

struct Asteroid {
  float theta;
  float radius;
  glm::mat4 model;
};

class PlanetApp : public Application {
 public:
  PlanetApp();
  void MainLoop() override;

 private:
  void Recreate();
  void GenAsteroidModels();
  void UpdateData(int frame);

  bool should_quit_ = false;
  int current_frame_ = 0;
  common::Timer timer_;
  std::unique_ptr<common::UserControlledCamera> camera_;
  std::unique_ptr<PerFrameCommand> command_;
  int num_asteroid_;
  std::unique_ptr<PerInstanceBuffer> per_asteroid_data_;
  std::unique_ptr<UniformBuffer> light_uniform_;
  std::unique_ptr<PushConstant> planet_constant_;
  std::unique_ptr<PushConstant> skybox_constant_;
  std::unique_ptr<Model> planet_model_, asteroid_model_, skybox_model_;
  std::unique_ptr<Image> depth_stencil_image_;
  std::unique_ptr<RenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
};

} /* namespace */

PlanetApp::PlanetApp() : Application{"Planet", WindowContext::Config{}} {
  // camera
  common::Camera::Config config;
  config.position = glm::vec3{1.6f, -5.1f, -5.9f};
  config.look_at = glm::vec3{-2.4f, -0.8f, 0.0f};
  camera_ = absl::make_unique<common::UserControlledCamera>(
      config, common::UserControlledCamera::ControlConfig{});

  using WindowKey = common::Window::KeyMap;
  using ControlKey = common::UserControlledCamera::ControlKey;
  window_context_.window()
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

  // command buffer
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFrameInFlight);

  // uniform buffer and push constants
  light_uniform_ = absl::make_unique<UniformBuffer>(
      context(), sizeof(Light), kNumFrameInFlight);
  planet_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(PlanetTrans), kNumFrameInFlight);
  skybox_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(SkyboxTrans), kNumFrameInFlight);

  // render pass builder
  render_pass_builder_ = naive_render_pass::GetNaiveRenderPassBuilder(
      context(), /*num_subpass=*/1, window_context_.num_swapchain_image(),
      window_context_.multisampling_mode());

  // model
  Descriptor::Info light_desc_info{
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      /*bindings=*/{{
          /*binding_point=*/1,
          /*array_length=*/1,
      }},
  };
  ModelBuilder::TextureBindingMap planet_bindings;
  planet_bindings[model::TextureType::kDiffuse] = {
      /*binding_point=*/2,
      {SharedTexture::SingleTexPath{GetResourcePath("texture/planet.png")}},
  };
  planet_model_ =
      ModelBuilder{
          context(), kNumFrameInFlight, /*is_opaque=*/true,
          ModelBuilder::SingleMeshResource{
              GetResourcePath("model/sphere.obj"),
              /*obj_index_base=*/1, planet_bindings},
      }
          .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                       GetShaderPath("vulkan/planet.vert.spv")})
          .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                       GetShaderPath("vulkan/planet.frag.spv")})
          .add_uniform_usage(Descriptor::Info{light_desc_info})
          .add_uniform_resource(
              /*binding_point=*/1, /*info_gen=*/[this](int frame) {
                return light_uniform_->GetDescriptorInfo(frame);
              })
          .set_push_constant({VK_SHADER_STAGE_VERTEX_BIT,
                              {{planet_constant_.get(), /*offset=*/0}}})
          .Build();

  GenAsteroidModels();
  vector<VertexAttribute> per_instance_attribs{
      {/*location=*/3, offsetof(Asteroid, theta), VK_FORMAT_R32_SFLOAT},
      {/*location=*/4, offsetof(Asteroid, radius), VK_FORMAT_R32_SFLOAT},
  };
  per_instance_attribs.reserve(6);
  int attrib_offset = offsetof(Asteroid, model);
  for (uint32_t location = 5; location <= 8; ++location) {
    per_instance_attribs.emplace_back(VertexAttribute{
        location, static_cast<uint32_t>(attrib_offset),
        VK_FORMAT_R32G32B32A32_SFLOAT,
    });
    attrib_offset += sizeof(glm::vec4);
  }
  light_desc_info.shader_stage |= VK_SHADER_STAGE_VERTEX_BIT;
  asteroid_model_ =
      ModelBuilder{
          context(), kNumFrameInFlight, /*is_opaque=*/true,
          ModelBuilder::MultiMeshResource{
              GetResourcePath("model/rock/rock.obj"),
              GetResourcePath("model/rock"),
              {{model::TextureType::kDiffuse, /*binding_point=*/2}}},
      }
          .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                       GetShaderPath("vulkan/asteroid.vert.spv")})
          .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                       GetShaderPath("vulkan/planet.frag.spv")})
          .add_instancing({per_instance_attribs,
                           static_cast<uint32_t>(sizeof(Asteroid)),
                           per_asteroid_data_.get()})
          .add_uniform_usage(Descriptor::Info{light_desc_info})
          .add_uniform_resource(
              /*binding_point=*/1, /*info_gen=*/[this](int frame) {
                return light_uniform_->GetDescriptorInfo(frame);
              })
          .set_push_constant({VK_SHADER_STAGE_VERTEX_BIT,
                              {{planet_constant_.get(), /*offset=*/0}}})
          .Build();

  ModelBuilder::TextureBindingMap skybox_bindings;
  skybox_bindings[model::TextureType::kCubemap] = {
      /*binding_point=*/1, {
          SharedTexture::CubemapPath{
              /*directory=*/GetResourcePath("texture/universe"),
              /*files=*/{"PositiveX.jpg", "NegativeX.jpg", "PositiveY.jpg",
                         "NegativeY.jpg", "PositiveZ.jpg", "NegativeZ.jpg"},
          },
      },
  };
  skybox_model_ =
      ModelBuilder{
          context(), kNumFrameInFlight, /*is_opaque=*/true,
          ModelBuilder::SingleMeshResource{
              GetResourcePath("model/skybox.obj"),
              /*obj_index_base=*/1, skybox_bindings},
      }
          .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                       GetShaderPath("vulkan/skybox.vert.spv")})
          .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                       GetShaderPath("vulkan/skybox.frag.spv")})
          .set_push_constant({VK_SHADER_STAGE_VERTEX_BIT,
                              {{skybox_constant_.get(), /*offset=*/0}}})
          .Build();
}

void PlanetApp::Recreate() {
  // depth stencil
  const VkExtent2D& frame_size = window_context_.frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
      context(), frame_size, window_context_.multisampling_mode());

  // render pass
  if (window_context_.multisampling_mode().has_value()) {
    render_pass_builder_->UpdateAttachmentImage(
        naive_render_pass::kMultisampleAttachmentIndex,
        [this](int framebuffer_index) -> const Image& {
          return window_context_.multisample_image();
        });
  }
  render_pass_ = (*render_pass_builder_)
      .SetFramebufferSize(frame_size)
      .UpdateAttachmentImage(
          naive_render_pass::kColorAttachmentIndex,
          [this](int framebuffer_index) -> const Image& {
            return window_context_.swapchain_image(framebuffer_index);
          })
      .UpdateAttachmentImage(
          naive_render_pass::kDepthStencilAttachmentIndex,
          [this](int framebuffer_index) -> const Image& {
            return *depth_stencil_image_;
          })
      .Build();

  // camera
  camera_->Calibrate(window_context_.window().GetScreenSize(),
                     window_context_.window().GetCursorPos());

  // model
  const auto sample_count = depth_stencil_image_->sample_count();
  planet_model_->Update(frame_size, sample_count,
                        *render_pass_, /*subpass_index=*/0);
  asteroid_model_->Update(frame_size, sample_count,
                          *render_pass_, /*subpass_index=*/0);
  skybox_model_->Update(frame_size, sample_count,
                        *render_pass_, /*subpass_index=*/0);
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
  vector<Asteroid> asteroids;
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

  per_asteroid_data_ = absl::make_unique<PerInstanceBuffer>(
      context(), asteroids.data(), sizeof(Asteroid) * asteroids.size());
}

void PlanetApp::UpdateData(int frame) {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();

  glm::vec3 light_dir{glm::sin(elapsed_time * 0.6f), -0.3f,
                      glm::cos(elapsed_time * 0.6f)};
  *light_uniform_->HostData<Light>(frame) =
      {glm::vec4{light_dir, elapsed_time}};
  light_uniform_->Flush(frame);

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(5.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  glm::mat4 view = camera_->view();
  glm::mat4 proj = camera_->projection();
  *planet_constant_->HostData<PlanetTrans>(frame) = {model, proj * view};
  *skybox_constant_->HostData<SkyboxTrans>(frame) = {proj, view};
}

void PlanetApp::MainLoop() {
  Recreate();
  const auto update_data = [this](int frame) {
    UpdateData(frame);
  };
  while (!should_quit_ && window_context_.CheckEvents()) {
    timer_.Tick();

    vector<RenderPass::RenderOp> render_ops{
        [&](const VkCommandBuffer& command_buffer) {
          planet_model_->Draw(command_buffer, current_frame_,
                              /*instance_count=*/1);
          asteroid_model_->Draw(command_buffer, current_frame_,
                                static_cast<uint32_t>(num_asteroid_));
          skybox_model_->Draw(command_buffer, current_frame_,
                              /*instance_count=*/1);
        },
    };
    auto draw_result = command_->Run(
        current_frame_, window_context_.swapchain(), update_data,
        [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
          render_pass_->Run(command_buffer, framebuffer_index, render_ops);
        });

    if (draw_result.has_value() || window_context_.ShouldRecreate()) {
      window_context_.Recreate();
      Recreate();
    }

    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
    camera_->SetActivity(true);  // not activated until first frame is displayed
  }
  context()->WaitIdle();  // wait for all async operations finish
}

} /* namespace planet */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<planet::PlanetApp>(argc, argv);
}
