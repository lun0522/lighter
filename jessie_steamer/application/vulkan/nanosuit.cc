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
  alignas(16) glm::mat4 view_model;
  alignas(16) glm::mat4 proj_view_model;
  alignas(16) glm::mat4 view_model_inv_trs;
  alignas(16) glm::mat4 view_inv;
};

struct SkyboxTrans {
  alignas(16) glm::mat4 proj;
  alignas(16) glm::mat4 view;
};

class NanosuitApp {
 public:
  NanosuitApp() : context_{Context::CreateContext()} {
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
  util::Timer timer_;
  std::shared_ptr<Context> context_;
  common::Camera camera_;
  Command command_;
  Model nanosuit_model_, skybox_model_;
  PushConstant nanosuit_constant_, skybox_constant_;
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
    common::Camera::Config config;
    config.pos = glm::vec3{0.0f, 3.5f, 12.0f};
    config.look_at = glm::vec3{0.0f, 3.5f, 0.0f};
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
    nanosuit_constant_.Init(sizeof(NanosuitTrans));
    skybox_constant_.Init(sizeof(SkyboxTrans));

    is_first_time = false;
  }

  // model
  Model::BindingPointMap nanosuit_bindings{
      {Model::TextureType::kTypeDiffuse, /*binding_point=*/1},
      {Model::TextureType::kTypeSpecular, /*binding_point=*/2},
      {Model::TextureType::kTypeReflection, /*binding_point=*/3},
  };
  // TODO: how to reuse?
  const std::string extra_dir{"jessie_steamer/resource/texture/tidepool/"};
  Model::TextureBindingMap extra_bindings;
  extra_bindings[Model::TextureType::kTypeSkybox] = {
      /*binding_point=*/4, {{
          extra_dir + "right.tga",
          extra_dir + "left.tga",
          extra_dir + "top.tga",
          extra_dir + "bottom.tga",
          extra_dir + "back.tga",
          extra_dir + "front.tga",
      }},
  };
  nanosuit_model_.Init(context_,
                       {{VK_SHADER_STAGE_VERTEX_BIT,
                         "jessie_steamer/shader/vulkan/nanosuit.vert.spv"},
                        {VK_SHADER_STAGE_FRAGMENT_BIT,
                         "jessie_steamer/shader/vulkan/nanosuit.frag.spv"}},
                       Model::MultiMeshResource{
                           "jessie_steamer/resource/model/nanosuit/"
                           "nanosuit.obj",
                           "jessie_steamer/resource/model/nanosuit",
                           nanosuit_bindings,
                       absl::make_optional<Model::TextureBindingMap>(
                           extra_bindings)},
                       /*uniform_infos=*/absl::nullopt,
                       /*instancing_info=*/absl::nullopt,
                       absl::make_optional<Model::PushConstantInfos>(
                           {{VK_SHADER_STAGE_VERTEX_BIT
                                 | VK_SHADER_STAGE_FRAGMENT_BIT,
                             {{&nanosuit_constant_, /*offset=*/0}}}}),
                       kNumFrameInFlight,
                       /*is_opaque=*/true);

  const std::string skybox_dir{"jessie_steamer/resource/texture/tidepool/"};
  Model::TextureBindingMap skybox_bindings;
  skybox_bindings[Model::TextureType::kTypeSkybox] = {
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
                       "jessie_steamer/shader/vulkan/skybox.vert.spv"},
                      {VK_SHADER_STAGE_FRAGMENT_BIT,
                       "jessie_steamer/shader/vulkan/skybox.frag.spv"}},
                     Model::SingleMeshResource{
                         "jessie_steamer/resource/model/skybox.obj",
                         /*obj_index_base=*/1, skybox_bindings},
                     /*uniform_infos=*/absl::nullopt,
                     /*instancing_info=*/absl::nullopt,
                     absl::make_optional<Model::PushConstantInfos>(
                         {{VK_SHADER_STAGE_VERTEX_BIT,
                           {{&skybox_constant_, /*offset=*/0}}}}),
                     kNumFrameInFlight,
                     /*is_opaque=*/true);

  // camera
  camera_.Calibrate(context_->window().screen_size(),
                    context_->window().cursor_pos());

  // command
  command_.Init(context_, kNumFrameInFlight);
}

void NanosuitApp::UpdateData(size_t frame_index) {
  const float elapsed_time = timer_.time_from_launch();

  glm::mat4 model{1.0f};
  model = glm::rotate(model, elapsed_time * glm::radians(90.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  model = glm::scale(model, glm::vec3{0.5f});

  glm::mat4 view = camera_.view();
  glm::mat4 proj = camera_.projection();
  glm::mat4 view_model = view * model;

  *nanosuit_constant_.data<NanosuitTrans>() = {
      view_model,
      proj * view_model,
      glm::transpose(glm::inverse(view_model)),
      glm::inverse(view),
  };

  *skybox_constant_.data<SkyboxTrans>() = {proj, view};
}

void NanosuitApp::MainLoop() {
  Init();
  const auto update_data = [this](size_t frame_index) {
    UpdateData(frame_index);
  };
  auto& window = context_->window();

  while (!should_quit_ && !window.ShouldQuit()) {
    const auto draw_result = command_.DrawFrame(
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

      nanosuit_model_.Draw(command_buffer, image_index, /*instance_count=*/1);
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
