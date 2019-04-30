//
//  nanosuit.cc
//
//  Created by Pujun Lun on 3/25/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/context.h"
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
namespace nanosuit {
namespace {

namespace util = common::util;
using namespace wrapper::vulkan;

constexpr size_t kNumFrameInFlight = 2;

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct NanosuitTrans {
  alignas(16) glm::mat4 proj_view_model;
};

struct SkyboxTrans {
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

class NanosuitApp {
 public:
  NanosuitApp()
    : context_{Context::CreateContext()},
      camera_{/*position=*/glm::vec3{0.0f, 1.5f, 4.0f}} {
    context_->Init("Nanosuit");
  };
  void MainLoop();

 private:
  void Init();
  void UpdateData(size_t frame_index);
  void Cleanup();

  bool should_quit_ = false;
  bool is_first_time = true;
  size_t current_frame_ = 0;
  util::TimePoint last_time_;
  std::shared_ptr<Context> context_;
  common::Camera camera_;
  Command command_;
  UniformBuffer nanosuit_uniform_, skybox_uniform_;
  Model nanosuit_model_, skybox_model_;
};

} /* namespace */

void NanosuitApp::Init() {
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
    nanosuit_uniform_.Init(context_, UniformBuffer::Info{
        sizeof(NanosuitTrans),
        context_->swapchain().size(),
    });

    skybox_uniform_.Init(context_, UniformBuffer::Info{
        sizeof(SkyboxTrans),
        context_->swapchain().size(),
    });

    is_first_time = false;
  }

  // model
  Descriptor::Info uniform_desc_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      /*shader_stage=*/VK_SHADER_STAGE_VERTEX_BIT,
      /*bindings=*/{{
          Descriptor::TextureType::kTypeMaxEnum,
          /*binding_point=*/0,
          /*array_length=*/1,
      }},
  };

  Model::BindingPointMap nanosuit_bindings{
      {Model::TextureType::kTypeDiffuse, /*binding_point=*/1},
      {Model::TextureType::kTypeSpecular, /*binding_point=*/2},
      {Model::TextureType::kTypeReflection, /*binding_point=*/3},
  };
  nanosuit_model_.Init(context_,
                       {{VK_SHADER_STAGE_VERTEX_BIT,
                         "jessie_steamer/shader/compiled/nanosuit.vert.spv"},
                        {VK_SHADER_STAGE_FRAGMENT_BIT,
                         "jessie_steamer/shader/compiled/nanosuit.frag.spv"}},
                       Model::MultiMeshResource{
                           "jessie_steamer/resource/model/nanosuit/"
                           "nanosuit.obj",
                           "jessie_steamer/resource/model/nanosuit",
                           nanosuit_bindings},
                       absl::make_optional<Model::UniformInfos>(
                           {{nanosuit_uniform_, uniform_desc_info}}),
                       /*instancing_info=*/absl::nullopt,
                       /*push_constants=*/nullptr, kNumFrameInFlight,
                       /*is_opaque=*/true);

  const std::string skybox_dir{"jessie_steamer/resource/texture/tidepool/"};
  Model::TextureBindingMap skybox_bindings;
  skybox_bindings[Model::TextureType::kTypeSpecular] = {
      /*binding_point=*/1, {{
           skybox_dir + "right.tga",
           skybox_dir + "left.tga",
           skybox_dir + "top.tga",
           skybox_dir + "bottom.tga",
           skybox_dir + "back.tga",
           skybox_dir + "front.tga",
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
                     absl::make_optional<Model::UniformInfos>(
                         {{skybox_uniform_, uniform_desc_info}}),
                     /*instancing_info=*/absl::nullopt,
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

    nanosuit_model_.Draw(command_buffer, image_index, /*instance_count=*/1);
    skybox_model_.Draw(command_buffer, image_index, /*instance_count=*/1);

    vkCmdEndRenderPass(command_buffer);
  });
}

void NanosuitApp::UpdateData(size_t frame_index) {
  static auto start_time = util::Now();
  auto elapsed_time = util::TimeInterval(start_time, util::Now());

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(90.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  model = glm::scale(model, glm::vec3{0.2f});

  glm::mat4 view = camera_.view_matrix();
  glm::mat4 proj = camera_.proj_matrix();
  // no need to flip Y-axis as OpenGL
  proj[1][1] *= -1;

  auto* nanosuit_trans = nanosuit_uniform_.data<NanosuitTrans>(frame_index);
  nanosuit_trans->proj_view_model = proj * view * model;

  auto* skybox_trans = skybox_uniform_.data<SkyboxTrans>(frame_index);
  skybox_trans->proj = proj;
  skybox_trans->view = view;
}

void NanosuitApp::MainLoop() {
  Init();
  const auto update_data = [this](size_t frame_index) {
    UpdateData(frame_index);
    nanosuit_uniform_.UpdateData(frame_index);
    skybox_uniform_.UpdateData(frame_index);
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

void NanosuitApp::Cleanup() {
  command_.Cleanup();
}

} /* namespace nanosuit */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, const char* argv[]) {
#ifdef DEBUG
  INSERT_DEBUG_REQUIREMENT(/*overwrite=*/true);
  jessie_steamer::application::vulkan::nanosuit::NanosuitApp app{};
  app.MainLoop();
#else
  try {
    jessie_steamer::application::vulkan::nanosuit::NanosuitApp app{};
    app.MainLoop();
  } catch (const std::exception& e) {
    std::cerr << "Error: /n/t" << e.what() << std::endl;
    return EXIT_FAILURE;
  }
#endif /* DEBUG */
  return EXIT_SUCCESS;
}
