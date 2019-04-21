//
//  nanosuit.cc
//
//  Created by Pujun Lun on 3/25/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
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
namespace nanosuit {
namespace {

namespace util = common::util;
using namespace wrapper::vulkan;
using std::vector;

constexpr size_t kNumFrameInFlight = 2;

class NanosuitApp {
 public:
  NanosuitApp() : context_{Context::CreateContext()} {
    context_->Init("Nanosuit");
  };
  void MainLoop();

 private:
  bool should_quit_ = false;
  bool is_first_time = true;
  size_t current_frame_ = 0;
  util::TimePoint last_time_;
  std::shared_ptr<Context> context_;
  common::Camera camera_;
  Command command_;
  UniformBuffer uniform_buffer_;
  DepthStencilImage depth_stencil_;
  Model nanosuit_model_, skybox_model_;

  void Init();
  void Cleanup();
  void UpdateTrans(size_t image_index);
};

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct Transformation {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

vector<Transformation> kTrans;

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
    kTrans.resize(context_->swapchain().size());
    UniformBuffer::Info chunk_info{
        kTrans.data(),
        sizeof(Transformation),
        CONTAINER_SIZE(kTrans),
    };
    uniform_buffer_.Init(context_, chunk_info);

    is_first_time = false;
  }

  // depth stencil
  depth_stencil_.Init(context_, context_->swapchain().extent());
  context_->render_pass().Config(depth_stencil_);

  // model
  Descriptor::Info uniform_desc_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      /*shader_stage=*/VK_SHADER_STAGE_VERTEX_BIT,
      /*bindings=*/{{
          Descriptor::TextureType::kTypeMaxEnum,
          /*binding_point=*/0,
          /*max_length=*/1,
      }},
  };
  std::vector<Model::UniformInfo> uniform_infos{
      {&uniform_buffer_, &uniform_desc_info},
  };

  Model::BindingPointMap nanosuit_bindings{
      {Model::TextureType::kTypeDiffuse, /*binding_point=*/1},
      {Model::TextureType::kTypeSpecular, /*binding_point=*/2},
      {Model::TextureType::kTypeReflection, /*binding_point=*/3},
  };
  nanosuit_model_.Init(context_,
                       "jessie_steamer/resource/model/nanosuit/nanosuit.obj",
                       "jessie_steamer/resource/model/nanosuit",
                       nanosuit_bindings, uniform_infos,
                       {{"jessie_steamer/shader/compiled/nanosuit.vert.spv",
                          VK_SHADER_STAGE_VERTEX_BIT},
                        {"jessie_steamer/shader/compiled/nanosuit.frag.spv",
                          VK_SHADER_STAGE_FRAGMENT_BIT}},
                       kNumFrameInFlight);

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
  skybox_model_.Init(context_, /*obj_index_base=*/1,
                     "jessie_steamer/resource/model/skybox.obj",
                     skybox_bindings, uniform_infos,
                     {{"jessie_steamer/shader/compiled/skybox.vert.spv",
                        VK_SHADER_STAGE_VERTEX_BIT},
                      {"jessie_steamer/shader/compiled/skybox.frag.spv",
                        VK_SHADER_STAGE_FRAGMENT_BIT}},
                     kNumFrameInFlight);

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
    //   - VK_SUBPASS_CONTENTS_INLINE: use primary commmand buffer
    //   - VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: use secondary
    vkCmdBeginRenderPass(command_buffer, &begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    nanosuit_model_.Draw(command_buffer, image_index);
    skybox_model_.Draw(command_buffer, image_index);

    vkCmdEndRenderPass(command_buffer);
  });
}

void NanosuitApp::Cleanup() {
  command_.Cleanup();
  nanosuit_model_.Cleanup();
  skybox_model_.Cleanup();
}

void NanosuitApp::UpdateTrans(size_t image_index) {
  glm::mat4 model{1.0f};
  model = glm::translate(model, glm::vec3{0.0f, -1.0f, -4.0f});
  static auto start_time = util::Now();
  auto elapsed_time = util::TimeInterval(start_time, util::Now());
  model = glm::rotate(model, elapsed_time * glm::radians(90.0f),
                      glm::vec3{0.0f, 1.0f, 0.0f});
  model = glm::scale(model, glm::vec3{0.2f});

  Transformation& trans = kTrans[image_index];
  trans = {std::move(model), camera_.view_matrix(), camera_.proj_matrix()};
  // no need to flip Y-axis as OpenGL
  trans.proj[1][1] *= -1;
}

void NanosuitApp::MainLoop() {
  Init();
  auto& window = context_->window();
  while (!should_quit_ && !window.ShouldQuit()) {
    window.PollEvents();
    last_time_ = util::Now();

    auto update_func = [this](size_t image_index) {
      UpdateTrans(image_index);
      uniform_buffer_.UpdateData(image_index);
    };
    if (command_.DrawFrame(current_frame_, update_func) != VK_SUCCESS ||
        window.IsResized()) {
      context_->WaitIdle();
      Cleanup();
      context_->Recreate();
      Init();
    }
    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
  }
  context_->WaitIdle(); // wait for all async operations finish
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
