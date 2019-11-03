//
//  editor.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor.h"

#include <vector>

#include "third_party/absl/memory/memory.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

constexpr int kObjFileIndexBase = 1;

enum SubpassIndex {
  kModelSubpassIndex = 0,
  kNumSubpasses,
};

enum EarthTextureIndex {
  kEarthDayTextureIndex = 0,
  kEarthNightTextureIndex,
};

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct EarthTrans {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

struct SkyboxTrans {
  ALIGN_MAT4 glm::mat4 proj;
  ALIGN_MAT4 glm::mat4 view;
};

struct TextureIndex {
  ALIGN_SCALAR(int32_t) int32_t value;
};

/* END: Consistent with uniform blocks defined in shaders. */

// Rotates the given 'model' matrix, so that if 'model' is glm::mat4{1.0f},
// the modified matrix will make the north pole point to the center of frame.
void RotateEarthModel(glm::mat4* model) {
  *model = glm::rotate(*model, glm::radians(-90.0f),
                       glm::vec3(1.0f, 0.0f, 0.0f));
  *model = glm::rotate(*model, glm::radians(-90.0f),
                       glm::vec3(0.0f, 1.0f, 0.0f));
}

} /* namespace */

Editor::Editor(const wrapper::vulkan::WindowContext& window_context,
               int num_frames_in_flight, common::Window* mutable_window)
    : context_{window_context.basic_context()} {
  using common::file::GetResourcePath;
  using common::file::GetVkShaderPath;
  using WindowKey = common::Window::KeyMap;
  using TextureType = ModelBuilder::TextureType;

  /* Camera */
  common::Camera::Config config;
  config.position = glm::vec3{0.0f, 0.0f, 3.0f};
  camera_ = absl::make_unique<common::UserControlledCamera>(
      config, common::UserControlledCamera::ControlConfig{});
  camera_->SetActivity(true);

  /* Window */
  (*mutable_window)
      .RegisterScrollCallback([this](double x_pos, double y_pos) {
        camera_->DidScroll(y_pos, 10.0f, 60.0f);
      })
      .RegisterPressKeyCallback(WindowKey::kUp, [this]() { is_day = true; })
      .RegisterPressKeyCallback(WindowKey::kDown, [this]() { is_day = false; });

  /* Uniform buffer and push constants */
  uniform_buffer_ = absl::make_unique<UniformBuffer>(
      context_, sizeof(EarthTrans), num_frames_in_flight);
  earth_constant_ = absl::make_unique<PushConstant>(
      context_, sizeof(TextureIndex), num_frames_in_flight);
  skybox_constant_ = absl::make_unique<PushConstant>(
      context_, sizeof(SkyboxTrans), num_frames_in_flight);

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/true,
      /*num_transparent_subpasses=*/0,
      /*num_overlay_subpasses=*/0,
  };
  render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
      context_, subpass_config,
      /*num_framebuffers=*/window_context.num_swapchain_images(),
      /*present_to_screen=*/true, window_context.multisampling_mode());

  /* Model */
  earth_model_ = ModelBuilder{
      context_, "earth", num_frames_in_flight,
      ModelBuilder::SingleMeshResource{
          GetResourcePath("model/sphere.obj"), kObjFileIndexBase,
          /*tex_source_map=*/{{
              TextureType::kDiffuse, {
                  SharedTexture::SingleTexPath{
                      GetResourcePath("texture/earth/day.jpg")},
                  SharedTexture::SingleTexPath{
                      GetResourcePath("texture/earth/night.jpg")},
              },
          }}
      }}
      .AddTextureBindingPoint(TextureType::kDiffuse, /*binding_point=*/2)
      .AddUniformBinding(
          VK_SHADER_STAGE_VERTEX_BIT,
          /*bindings=*/{{/*binding_point=*/0, /*array_length=*/1}})
      .AddUniformBuffer(/*binding_point=*/0, *uniform_buffer_)
      .SetPushConstantShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT)
      .AddPushConstant(earth_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT, GetVkShaderPath("earth.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT, GetVkShaderPath("earth.frag"))
      .Build();

  const SharedTexture::CubemapPath skybox_path{
      /*directory=*/GetResourcePath("texture/universe"),
      /*files=*/{
          "PositiveX.jpg", "NegativeX.jpg",
          "PositiveY.jpg", "NegativeY.jpg",
          "PositiveZ.jpg", "NegativeZ.jpg",
      },
  };

  skybox_model_ = ModelBuilder{
      context_, "skybox", num_frames_in_flight,
      ModelBuilder::SingleMeshResource{
          GetResourcePath("model/skybox.obj"), kObjFileIndexBase,
          {{TextureType::kCubemap, {skybox_path}}},
      }}
      .AddTextureBindingPoint(TextureType::kCubemap, /*binding_point=*/1)
      .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
      .AddPushConstant(skybox_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT, GetVkShaderPath("skybox.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT, GetVkShaderPath("skybox.frag"))
      .Build();
}

void Editor::Recreate(const wrapper::vulkan::WindowContext& window_context) {
  /* Camera */
  camera_->Calibrate(window_context.window().GetScreenSize(),
                     window_context.window().GetCursorPos());

  /* Depth image */
  const VkExtent2D& frame_size = window_context.frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
      context_, frame_size, window_context.multisampling_mode());

  /* Render pass */
  (*render_pass_builder_->mutable_builder())
      .UpdateAttachmentImage(
          render_pass_builder_->color_attachment_index(),
          [&window_context](int framebuffer_index) -> const Image& {
            return window_context.swapchain_image(framebuffer_index);
          })
      .UpdateAttachmentImage(
          render_pass_builder_->depth_attachment_index(),
          [this](int framebuffer_index) -> const Image& {
            return *depth_stencil_image_;
          });
  if (render_pass_builder_->has_multisample_attachment()) {
    render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
        render_pass_builder_->multisample_attachment_index(),
        [&window_context](int framebuffer_index) -> const Image& {
          return window_context.multisample_image();
        });
  }
  render_pass_ = (*render_pass_builder_)->Build();

  /* Model */
  constexpr bool kIsObjectOpaque = true;
  const VkSampleCountFlagBits sample_count = window_context.sample_count();
  earth_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                       *render_pass_, kModelSubpassIndex);
  skybox_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                        *render_pass_, kModelSubpassIndex);
}

void Editor::UpdateData(int frame) {
  const glm::mat4& proj = camera_->projection();
  const glm::mat4& view = camera_->view();
  auto earth_model = glm::mat4{1.0f};
  RotateEarthModel(&earth_model);
  uniform_buffer_->HostData<EarthTrans>(frame)->proj_view_model =
      proj * view * earth_model;
  uniform_buffer_->Flush(frame);

  earth_constant_->HostData<TextureIndex>(frame)->value =
      is_day ? kEarthDayTextureIndex : kEarthNightTextureIndex;
  *skybox_constant_->HostData<SkyboxTrans>(frame) = {proj, view * earth_model};
}

void Editor::Render(const VkCommandBuffer& command_buffer,
                    uint32_t framebuffer_index, int current_frame) {
  render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
      [&](const VkCommandBuffer& command_buffer) {
        earth_model_->Draw(command_buffer, current_frame, /*instance_count=*/1);
        skybox_model_->Draw(command_buffer, current_frame,
                            /*instance_count=*/1);
      },
  });
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
