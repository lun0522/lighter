//
//  nanosuit.cc
//
//  Created by Pujun Lun on 3/25/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "nanosuit.h"

#include <array>
#include <chrono>

#define GLM_FORCE_RADIANS
// different from OpenGL, where depth values are in range [-1.0, 1.0]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include "window.h"

namespace application {
namespace vulkan {
namespace {

using std::vector;
using window::key_map::KeyMap;

constexpr size_t kNumFrameInFlight = 2;

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
    auto& window = context_->window();
    window.SetCursorHidden(true);
    window.RegisterKeyCallback(KeyMap::kEscape,
                               [this]() { should_quit_ = true; });

    // camera
    camera_ = std::make_unique<camera::Camera>();
    window.RegisterCursorMoveCallback([this](double x_pos, double y_pos) {
      camera_->ProcessCursorMove(x_pos, y_pos);
    });
    window.RegisterScrollCallback([this](double x_pos, double y_pos) {
      camera_->ProcessScroll(y_pos, 1.0f, 60.0f);
    });
    window.RegisterKeyCallback(KeyMap::kUp, [this]() {
      camera_->ProcessKey(KeyMap::kUp,
                          util::TimeInterval(last_time_, util::Now()));
    });
    window.RegisterKeyCallback(KeyMap::kDown, [this]() {
      camera_->ProcessKey(KeyMap::kDown,
                          util::TimeInterval(last_time_, util::Now()));
    });
    window.RegisterKeyCallback(KeyMap::kLeft, [this]() {
      camera_->ProcessKey(KeyMap::kLeft,
                          util::TimeInterval(last_time_, util::Now()));
    });
    window.RegisterKeyCallback(KeyMap::kRight, [this]() {
      camera_->ProcessKey(KeyMap::kRight,
                          util::TimeInterval(last_time_, util::Now()));
    });

    // model (vertex buffer)
    cube_model_.Init(context_->ptr(), "texture/cube.obj", 1);
    skybox_model_.Init(context_->ptr(), "texture/skybox.obj", 1);

    // uniform buffer
    kTrans.resize(context_->swapchain().size());
    buffer::ChunkInfo chunk_info{
        kTrans.data(),
        sizeof(Transformation),
        CONTAINER_SIZE(kTrans),
    };
    uniform_buffer_.Init(context_->ptr(), chunk_info);

    // texture
    cube_tex_.Init(context_, {"texture/statue.jpg"});
    const std::string skybox_dir{"texture/tidepool/"};
    skybox_tex_.Init(context_, {skybox_dir + "right.tga",
                                skybox_dir + "left.tga",
                                skybox_dir + "top.tga",
                                skybox_dir + "bottom.tga",
                                skybox_dir + "back.tga",
                                skybox_dir + "front.tga"});

    // descriptor
    cude_rsrc_infos_ = {
        descriptor::ResourceInfo{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {0},
                                 VK_SHADER_STAGE_VERTEX_BIT},
        descriptor::ResourceInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {1},
                                 VK_SHADER_STAGE_FRAGMENT_BIT},
    };
    cube_dscs_.reserve(kNumFrameInFlight);
    for (size_t i = 0; i < kNumFrameInFlight; ++i) {
      cube_dscs_.emplace_back(std::make_unique<Descriptor>());
      cube_dscs_[i]->Init(context_, cude_rsrc_infos_);
      cube_dscs_[i]->UpdateBufferInfos(cude_rsrc_infos_[0],
                                       {uniform_buffer_.descriptor_info(i)});
      cube_dscs_[i]->UpdateImageInfos(cude_rsrc_infos_[1],
                                      {cube_tex_.descriptor_info()});
    }

    skybox_rsrc_infos_ = {
        descriptor::ResourceInfo{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {0},
                                 VK_SHADER_STAGE_VERTEX_BIT},
        descriptor::ResourceInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {1},
                                 VK_SHADER_STAGE_FRAGMENT_BIT},
    };
    skybox_dscs_.reserve(kNumFrameInFlight);
    for (size_t i = 0; i < kNumFrameInFlight; ++i) {
      skybox_dscs_.emplace_back(std::make_unique<Descriptor>());
      skybox_dscs_[i]->Init(context_, skybox_rsrc_infos_);
      skybox_dscs_[i]->UpdateBufferInfos(skybox_rsrc_infos_[0],
                                         {uniform_buffer_.descriptor_info(i)});
      skybox_dscs_[i]->UpdateImageInfos(skybox_rsrc_infos_[1],
                                        {skybox_tex_.descriptor_info()});
    }

    is_first_time = false;
  }

  // time
  last_time_ = util::Now();

  // camera
  camera_->Init(context_->window().screen_size(),
                context_->window().cursor_pos());

  // depth stencil
  depth_stencil_.Init(context_, context_->swapchain().extent());
  context_->render_pass().Config(depth_stencil_);

  // pipeline
  cube_pipeline_.Init(
      context_->ptr(),
      {{"compiled/simple.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
       {"compiled/simple.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}},
      cube_dscs_[0]->layout(), cube_model_.binding_descs(),
      cube_model_.attrib_descs());

  skybox_pipeline_.Init(
      context_->ptr(),
      {{"compiled/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
       {"compiled/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}},
      skybox_dscs_[0]->layout(), skybox_model_.binding_descs(),
      skybox_model_.attrib_descs());

  // command
  command_.Init(context_->ptr(), kNumFrameInFlight,
                [&](const VkCommandBuffer& command_buffer, size_t image_index) {
    // start render pass
    std::array<VkClearValue, 2> clear_values;
    clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
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

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      *cube_pipeline_);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            cube_pipeline_.layout(), 0, 1,
                            &cube_dscs_[image_index]->set(), 0, nullptr);
    cube_model_.Draw(command_buffer);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      *skybox_pipeline_);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            skybox_pipeline_.layout(), 0, 1,
                            &skybox_dscs_[image_index]->set(), 0, nullptr);
    skybox_model_.Draw(command_buffer);

    vkCmdEndRenderPass(command_buffer);
  });
}

void NanosuitApp::Cleanup() {
  command_.Cleanup();
  cube_pipeline_.Cleanup();
  skybox_pipeline_.Cleanup();
}

void NanosuitApp::UpdateTrans(size_t image_index) {
  glm::mat4 model{1.0f};
  model = glm::translate(model, glm::vec3{0.0f, 0.0f, -5.0f});
  static auto start_time = util::Now();
  auto elapsed_time = util::TimeInterval(start_time, util::Now());
  model = glm::rotate(model, elapsed_time * glm::radians(90.0f),
                      glm::vec3{1.0f});

  Transformation& trans = kTrans[image_index];
  trans = {std::move(model), camera_->view_matrix(), camera_->proj_matrix()};
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
      uniform_buffer_.Update(image_index);
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

} /* namespace vulkan */
} /* namespace application */
