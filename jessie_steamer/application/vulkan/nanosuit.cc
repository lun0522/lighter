//
//  nanosuit.cc
//
//  Created by Pujun Lun on 3/25/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <iostream>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"
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
namespace nanosuit {
namespace {

using namespace wrapper::vulkan;

constexpr int kNumFrameInFlight = 2;

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct NanosuitVertTrans {
  alignas(16) glm::mat4 view_model;
  alignas(16) glm::mat4 proj_view_model;
  alignas(16) glm::mat4 view_model_inv_trs;
};

struct NanosuitFragTrans {
  alignas(16) glm::mat4 view_inv;
};

struct SkyboxTrans {
  alignas(16) glm::mat4 proj;
  alignas(16) glm::mat4 view;
};

class NanosuitApp : public Application {
 public:
  NanosuitApp() : command_{context()}, nanosuit_model_{context()},
                  skybox_model_{context()}, nanosuit_vert_uniform_{context()} {}
  void MainLoop() override;

 private:
  void Init();
  void UpdateData(int frame);

  bool should_quit_ = false;
  bool is_first_time = true;
  int current_frame_ = 0;
  common::Timer timer_;
  common::Camera camera_;
  PerFrameCommand command_;
  Model nanosuit_model_, skybox_model_;
  UniformBuffer nanosuit_vert_uniform_;
  PushConstant nanosuit_frag_constant_, skybox_constant_;
  std::unique_ptr<DepthStencilImage> depth_stencil_;
  std::unique_ptr<RenderPass> render_pass_;
};

} /* namespace */

void NanosuitApp::Init() {
  // window
  window_context_.Init("Nanosuit");

  if (is_first_time) {
    is_first_time = false;

    using KeyMap = common::Window::KeyMap;

    auto& window = window_context_.window();
    window.SetCursorHidden(true);
    window.RegisterKeyCallback(KeyMap::kEscape,
                               [this]() { should_quit_ = true; });

    // camera
    common::Camera::Config config;
    config.pos = glm::vec3{0.0f, 3.5f, 12.0f};
    config.look_at = glm::vec3{0.0f, 3.5f, 0.0f};
    config.lock_look_at = true;
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
    nanosuit_vert_uniform_.Init(sizeof(NanosuitVertTrans), kNumFrameInFlight);
    nanosuit_frag_constant_.Init(sizeof(NanosuitFragTrans), kNumFrameInFlight);
    skybox_constant_.Init(sizeof(SkyboxTrans), kNumFrameInFlight);
  }

  // depth stencil
  auto frame_size = window_context_.frame_size();
  depth_stencil_ = absl::make_unique<DepthStencilImage>(
      context(), window_context_.frame_size());

  // render pass
  render_pass_ = RenderPassBuilder::DefaultBuilder(
      context(), window_context_.swapchain(), *depth_stencil_)
          .Build(window_context_.swapchain(), *depth_stencil_);

  // model
  TextureImage::CubemapPath skybox_path;
  Model::TextureBindingMap skybox_bindings;
  skybox_bindings[Model::ResourceType::kTextureCubemap] = {
      /*binding_point=*/4, {
          TextureImage::CubemapPath{
              /*directory=*/"external/resource/texture/tidepool",
              /*files=*/{
                  "right.tga",
                  "left.tga",
                  "top.tga",
                  "bottom.tga",
                  "back.tga",
                  "front.tga",
              },
          },
      },
  };

  Model::BindingPointMap nanosuit_bindings{
      {Model::ResourceType::kTextureDiffuse, /*binding_point=*/1},
      {Model::ResourceType::kTextureSpecular, /*binding_point=*/2},
      {Model::ResourceType::kTextureReflection, /*binding_point=*/3},
  };
  Descriptor::Info trans_desc_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      /*shader_stage=*/VK_SHADER_STAGE_VERTEX_BIT,
      /*bindings=*/{{
          Descriptor::ResourceType::kUniformBuffer,
          /*binding_point=*/0,
          /*array_length=*/1,
      }},
  };
  nanosuit_model_.Init({{VK_SHADER_STAGE_VERTEX_BIT,
                         "jessie_steamer/shader/vulkan/nanosuit.vert.spv"},
                        {VK_SHADER_STAGE_FRAGMENT_BIT,
                         "jessie_steamer/shader/vulkan/nanosuit.frag.spv"}},
                       Model::MultiMeshResource{
                           "external/resource/model/nanosuit/nanosuit.obj",
                           "external/resource/model/nanosuit",
                           nanosuit_bindings,
                           absl::make_optional<Model::TextureBindingMap>(
                               skybox_bindings)},
                       absl::make_optional<Model::UniformInfos>(
                           {{nanosuit_vert_uniform_, trans_desc_info}}),
                       /*instancing_info=*/absl::nullopt,
                       absl::make_optional<Model::PushConstantInfos>(
                           {{VK_SHADER_STAGE_FRAGMENT_BIT,
                             {{&nanosuit_frag_constant_, /*offset=*/0}}}}),
                       {**render_pass_, /*subpass=*/0},
                       frame_size, kNumFrameInFlight, /*is_opaque=*/true);

  skybox_bindings[Model::ResourceType::kTextureCubemap].binding_point = 1;
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

void NanosuitApp::UpdateData(int frame) {
  const float elapsed_time = timer_.time_from_launch();

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(90.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  model = glm::scale(model, glm::vec3{0.5f});

  glm::mat4 view = camera_.view();
  glm::mat4 proj = camera_.projection();
  glm::mat4 view_model = view * model;

  *nanosuit_vert_uniform_.data<NanosuitVertTrans>(frame) = {
      view_model,
      proj * view_model,
      glm::transpose(glm::inverse(view_model)),
  };
  nanosuit_vert_uniform_.Flush(frame);

  *nanosuit_frag_constant_.data<NanosuitFragTrans>(frame) =
      {glm::inverse(view)};
  *skybox_constant_.data<SkyboxTrans>(frame) = {proj, view};
}

void NanosuitApp::MainLoop() {
  Init();
  const auto update_data = [this](int frame) {
    UpdateData(frame);
  };
  while (!should_quit_ && !window_context_.ShouldQuit()) {
    const VkExtent2D frame_size = window_context_.frame_size();
    const auto draw_result = command_.Run(
        current_frame_, *window_context_.swapchain(), update_data,
        [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
          render_pass_->Run(
              command_buffer, framebuffer_index, frame_size,
              [this, &command_buffer]() {
                nanosuit_model_.Draw(command_buffer, current_frame_,
                                     /*instance_count=*/1);
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

} /* namespace nanosuit */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, const char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<nanosuit::NanosuitApp>();
}
