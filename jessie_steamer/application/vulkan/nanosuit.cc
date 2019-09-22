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
#include "jessie_steamer/common/file.h"
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

using common::file::GetResourcePath;
using common::file::GetShaderPath;

constexpr int kNumFrameInFlight = 2;

struct NanosuitVertTrans {
  ALIGN_MAT4 glm::mat4 view_model;
  ALIGN_MAT4 glm::mat4 proj_view_model;
  ALIGN_MAT4 glm::mat4 view_model_inv_trs;
};

struct NanosuitFragTrans {
  ALIGN_MAT4 glm::mat4 view_inv;
};

struct SkyboxTrans {
  ALIGN_MAT4 glm::mat4 proj;
  ALIGN_MAT4 glm::mat4 view;
};

class NanosuitApp : public Application {
 public:
  NanosuitApp();
  void MainLoop() override;

 private:
  void Recreate();
  void UpdateData(int frame);

  bool should_quit_ = false;
  int current_frame_ = 0;
  common::Timer timer_;
  std::unique_ptr<common::UserControlledCamera> camera_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<Model> nanosuit_model_, skybox_model_;
  std::unique_ptr<UniformBuffer> nanosuit_vert_uniform_;
  std::unique_ptr<PushConstant> nanosuit_frag_constant_;
  std::unique_ptr<PushConstant> skybox_constant_;
  std::unique_ptr<Image> depth_stencil_image_;
  std::unique_ptr<RenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
};

} /* namespace */

NanosuitApp::NanosuitApp() : Application{"Nanosuit", WindowContext::Config{}} {
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
  nanosuit_vert_uniform_ = absl::make_unique<UniformBuffer>(
      context(), sizeof(NanosuitVertTrans), kNumFrameInFlight);
  nanosuit_frag_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(NanosuitFragTrans), kNumFrameInFlight);
  skybox_constant_ = absl::make_unique<PushConstant>(
      context(), sizeof(SkyboxTrans), kNumFrameInFlight);

  // render pass builder
  render_pass_builder_ = RenderPassBuilder::SimpleRenderPassBuilder(
      context(), /*num_subpass=*/1, window_context_.num_swapchain_image(),
      window_context_.multisampling_mode());

  // model
  ModelBuilder::TextureBinding skybox_binding{
      /*binding_point=*/4, {
          SharedTexture::CubemapPath{
              /*directory=*/GetResourcePath("texture/tidepool"),
              /*files=*/{"right.tga", "left.tga", "top.tga", "bottom.tga",
                         "back.tga", "front.tga"},
          },
      },
  };

  ModelBuilder::BindingPointMap nanosuit_bindings{
      {model::TextureType::kDiffuse, /*binding_point=*/1},
      {model::TextureType::kSpecular, /*binding_point=*/2},
      {model::TextureType::kReflection, /*binding_point=*/3},
  };
  Descriptor::Info trans_desc_info{
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_VERTEX_BIT,
      /*bindings=*/{{
          /*binding_point=*/0,
          /*array_length=*/1,
      }},
  };
  nanosuit_model_ =
      ModelBuilder{
          context(), kNumFrameInFlight, /*is_opaque=*/true,
          ModelBuilder::MultiMeshResource{
              GetResourcePath("model/nanosuit/nanosuit.obj"),
              GetResourcePath("model/nanosuit"),
              nanosuit_bindings},
      }
          .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                       GetShaderPath("vulkan/nanosuit.vert.spv")})
          .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                       GetShaderPath("vulkan/nanosuit.frag.spv")})
          .add_uniform_usage(std::move(trans_desc_info))
          .add_uniform_resource(
              /*binding_point=*/0, /*info_gen=*/[this](int frame) {
                return nanosuit_vert_uniform_->GetDescriptorInfo(frame);
              })
          .set_push_constant({VK_SHADER_STAGE_FRAGMENT_BIT,
                              {{nanosuit_frag_constant_.get(), /*offset=*/0}}})
          .add_shared_texture(Descriptor::TextureType::kCubemap, skybox_binding)
          .Build();

  skybox_binding.binding_point = 1;
  skybox_model_ =
      ModelBuilder{
          context(), kNumFrameInFlight, /*is_opaque=*/true,
          ModelBuilder::SingleMeshResource{
              GetResourcePath("model/skybox.obj"),
              /*obj_index_base=*/1,
              {{model::TextureType::kCubemap, skybox_binding}}},
      }
          .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                       GetShaderPath("vulkan/skybox.vert.spv")})
          .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                       GetShaderPath("vulkan/skybox.frag.spv")})
          .set_push_constant({VK_SHADER_STAGE_VERTEX_BIT,
                              {{skybox_constant_.get(), /*offset=*/0}}})
          .Build();
}

void NanosuitApp::Recreate() {
  // depth stencil
  const auto frame_size = window_context_.frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
      context(), frame_size, window_context_.multisampling_mode());

  // render pass
  if (window_context_.multisampling_mode() != absl::nullopt) {
    render_pass_builder_->update_image(
        simple_render_pass::kMultisampleAttachmentIndex,
        [this](int index) -> const Image& {
          return window_context_.multisample_image();
        });
  }
  render_pass_ = (*render_pass_builder_)
      .set_framebuffer_size(frame_size)
      .update_image(simple_render_pass::kColorAttachmentIndex,
                    [this](int index) -> const Image& {
                      return window_context_.swapchain_image(index);
                    })
      .update_image(simple_render_pass::kDepthStencilAttachmentIndex,
                    [this](int index) -> const Image& {
                      return *depth_stencil_image_;
                    })
      .Build();

  // camera
  camera_->Calibrate(window_context_.window().GetScreenSize(),
                     window_context_.window().GetCursorPos());

  // model
  const auto sample_count = depth_stencil_image_->sample_count();
  nanosuit_model_->Update(frame_size, sample_count,
                          *render_pass_, /*subpass_index=*/0);
  skybox_model_->Update(frame_size, sample_count,
                        *render_pass_, /*subpass_index=*/0);
}

void NanosuitApp::UpdateData(int frame) {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(90.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  model = glm::scale(model, glm::vec3{0.5f});

  glm::mat4 view = camera_->view();
  glm::mat4 proj = camera_->projection();
  glm::mat4 view_model = view * model;

  *nanosuit_vert_uniform_->HostData<NanosuitVertTrans>(frame) = {
      view_model,
      proj * view_model,
      glm::transpose(glm::inverse(view_model)),
  };
  nanosuit_vert_uniform_->Flush(frame);

  *nanosuit_frag_constant_->HostData<NanosuitFragTrans>(frame) =
      {glm::inverse(view)};
  *skybox_constant_->HostData<SkyboxTrans>(frame) = {proj, view};
}

void NanosuitApp::MainLoop() {
  Recreate();
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
    const auto draw_result = command_->Run(
        current_frame_, window_context_.swapchain(), update_data,
        [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
          render_pass_->Run(command_buffer, framebuffer_index, render_ops);
        });

    if (draw_result != VK_SUCCESS || window_context_.ShouldRecreate()) {
      window_context_.Recreate();
      Recreate();
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

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<nanosuit::NanosuitApp>(argc, argv);
}
