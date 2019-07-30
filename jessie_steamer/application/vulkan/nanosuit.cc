//
//  nanosuit.cc
//
//  Created by Pujun Lun on 3/25/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/time.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
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
constexpr MultisampleImage::Mode kMultisamplingMode =
    MultisampleImage::Mode::kEfficient;

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
  NanosuitApp() : command_{context()}, nanosuit_vert_uniform_{context()} {}
  void MainLoop() override;

 private:
  void Init();
  void UpdateData(int frame);

  bool should_quit_ = false;
  bool is_first_time = true;
  int current_frame_ = 0;
  common::Timer timer_;
  std::unique_ptr<common::UserControlledCamera> camera_;
  PerFrameCommand command_;
  std::unique_ptr<Model> nanosuit_model_, skybox_model_;
  UniformBuffer nanosuit_vert_uniform_;
  PushConstant nanosuit_frag_constant_, skybox_constant_;
  std::unique_ptr<MultisampleImage> depth_stencil_image_;
  std::unique_ptr<MultisampleImage> multisample_image_;
  std::unique_ptr<RenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
};

} /* namespace */

void NanosuitApp::Init() {
  // window
  window_context_.Init("Nanosuit");

  // multisampling
  multisample_image_ = MultisampleImage::CreateColorMultisampleImage(
      context(), window_context_.swapchain_image(0), kMultisamplingMode);

  // depth stencil
  auto frame_size = window_context_.frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilMultisampleImage(
      context(), frame_size, kMultisamplingMode);

  if (is_first_time) {
    is_first_time = false;

    // camera
    common::Camera::Config config;
    common::UserControlledCamera::ControlConfig control_config;
    config.position = glm::vec3{0.0f, 3.5f, 12.0f};
    config.look_at = glm::vec3{0.0f, 3.5f, 0.0f};
    control_config.lock_center = true;
    camera_ = absl::make_unique<common::UserControlledCamera>(
        config, control_config);

    using WindowKey = common::Window::KeyMap;
    using ControlKey = common::UserControlledCamera::ControlKey;
    window_context_.window()
        .SetCursorHidden(true)
        .RegisterCursorMoveCallback([this](double x_pos, double y_pos) {
          camera_->ProcessCursorMove(x_pos, y_pos);
        })
        .RegisterScrollCallback([this](double x_pos, double y_pos) {
          camera_->ProcessScroll(y_pos, 1.0f, 60.0f);
        })
        .RegisterKeyCallback(WindowKey::kUp, [this]() {
          camera_->ProcessKey(ControlKey::kUp, timer_.time_from_last_frame());
        })
        .RegisterKeyCallback(WindowKey::kDown, [this]() {
          camera_->ProcessKey(ControlKey::kDown, timer_.time_from_last_frame());
        })
        .RegisterKeyCallback(WindowKey::kLeft, [this]() {
          camera_->ProcessKey(ControlKey::kLeft, timer_.time_from_last_frame());
        })
        .RegisterKeyCallback(WindowKey::kRight, [this]() {
          camera_->ProcessKey(ControlKey::kRight,
                              timer_.time_from_last_frame());
        })
        .RegisterKeyCallback(WindowKey::kEscape,
                             [this]() { should_quit_ = true; });

    // push constants
    nanosuit_vert_uniform_.Init(sizeof(NanosuitVertTrans), kNumFrameInFlight);
    nanosuit_frag_constant_.Init(sizeof(NanosuitFragTrans), kNumFrameInFlight);
    skybox_constant_.Init(sizeof(SkyboxTrans), kNumFrameInFlight);

    // render pass builder
    render_pass_builder_ = RenderPassBuilder::SimpleRenderPassBuilder(
        context(), /*num_subpass=*/1,
        /*get_depth_stencil_image=*/[this](int index) -> const Image& {
          return *depth_stencil_image_;
        },
        window_context_.num_swapchain_image(),
        /*get_swapchain_image=*/[this](int index) -> const Image& {
          return window_context_.swapchain_image(index);
        },
        /*get_multisample_image=*/[this](int index) -> const Image& {
          return *multisample_image_;
        });

    // model
    ModelBuilder::TextureBinding skybox_binding{
        /*binding_point=*/4, {
            SharedTexture::CubemapPath{
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

    ModelBuilder::BindingPointMap nanosuit_bindings{
        {model::ResourceType::kTextureDiffuse, /*binding_point=*/1},
        {model::ResourceType::kTextureSpecular, /*binding_point=*/2},
        {model::ResourceType::kTextureReflection, /*binding_point=*/3},
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
    nanosuit_model_ =
        ModelBuilder{
            context(), kNumFrameInFlight, /*is_opaque=*/true,
            ModelBuilder::MultiMeshResource{
                "external/resource/model/nanosuit/nanosuit.obj",
                "external/resource/model/nanosuit",
                nanosuit_bindings},
        }
        .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                     "jessie_steamer/shader/vulkan/nanosuit.vert.spv"})
        .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                     "jessie_steamer/shader/vulkan/nanosuit.frag.spv"})
        .add_uniform_buffer({&nanosuit_vert_uniform_, trans_desc_info})
        .add_push_constant({VK_SHADER_STAGE_FRAGMENT_BIT,
                            {{&nanosuit_frag_constant_, /*offset=*/0}}})
        .add_shared_texture(Descriptor::ResourceType::kTextureCubemap,
                            skybox_binding)
        .set_sample_count(depth_stencil_image_->sample_count())
        .Build();

    skybox_binding.binding_point = 1;
    skybox_model_ =
        ModelBuilder{
            context(), kNumFrameInFlight, /*is_opaque=*/true,
            ModelBuilder::SingleMeshResource{
                "external/resource/model/skybox.obj",
                /*obj_index_base=*/1,
                {{model::ResourceType::kTextureCubemap, skybox_binding}}},
        }
        .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                     "jessie_steamer/shader/vulkan/skybox.vert.spv"})
        .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                     "jessie_steamer/shader/vulkan/skybox.frag.spv"})
        .add_push_constant({VK_SHADER_STAGE_VERTEX_BIT,
                            {{&skybox_constant_, /*offset=*/0}}})
        .set_sample_count(depth_stencil_image_->sample_count())
        .Build();
  }

  // render pass
  render_pass_ = (*render_pass_builder_)
      .set_framebuffer_size(frame_size)
      .Build();

  // camera
  camera_->Calibrate(window_context_.window().GetScreenSize(),
                     window_context_.window().GetCursorPos());

  // model
  nanosuit_model_->Update(frame_size, *render_pass_, /*subpass_index=*/0);
  skybox_model_->Update(frame_size, *render_pass_, /*subpass_index=*/0);

  // command
  command_.Init(kNumFrameInFlight, &context()->queues());
}

void NanosuitApp::UpdateData(int frame) {
  const float elapsed_time = timer_.time_from_launch();

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(90.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  model = glm::scale(model, glm::vec3{0.5f});

  glm::mat4 view = camera_->view();
  glm::mat4 proj = camera_->projection();
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
  while (!should_quit_ && window_context_.CheckEvents()) {
    timer_.Tick();

    std::vector<RenderPass::RenderOp> render_ops{
        [&](const VkCommandBuffer& command_buffer) {
          nanosuit_model_->Draw(command_buffer, current_frame_,
                                /*instance_count=*/1);
          skybox_model_->Draw(command_buffer, current_frame_,
                              /*instance_count=*/1);
        },
    };
    const auto draw_result = command_.Run(
        current_frame_, window_context_.swapchain(), update_data,
        [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
          render_pass_->Run(command_buffer, framebuffer_index, render_ops);
        });

    if (draw_result != VK_SUCCESS) {
      window_context_.Cleanup();
      command_.Cleanup();
      Init();
    }

    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
    camera_->SetActivity(true);  // not activated until first frame is displayed
  }
  context()->WaitIdle();  // wait for all async operations finish
}

} /* namespace nanosuit */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, const char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<nanosuit::NanosuitApp>();
}
