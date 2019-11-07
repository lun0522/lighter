//
//  editor.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor.h"

#include <cmath>
#include <vector>

#include "third_party/absl/memory/memory.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "third_party/glm/gtx/intersect.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

constexpr int kObjFileIndexBase = 1;

// The height of aurora layer is assumed to be at around 100km above the ground.
constexpr float kEarthModelRadius = 1.0f;
constexpr float kEarthRadius = 6378.1f;
constexpr float kAuroraHeight = 100.0f;
constexpr float kAuroraLayerRadius =
    (kEarthRadius + kAuroraHeight) / kEarthRadius * kEarthModelRadius;

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
  ALIGN_MAT4 glm::mat4 view_model;
};

struct TextureIndex {
  ALIGN_SCALAR(int32_t) int32_t value;
};

/* END: Consistent with uniform blocks defined in shaders. */

inline glm::vec3 TransformPoint(const glm::mat4& transform,
                                const glm::vec3& point) {
  const glm::vec4 transformed = transform * glm::vec4{point, 1.0f};
  return glm::vec3{transformed} / transformed.w;
}

} /* namespace */

Editor::Editor(const wrapper::vulkan::WindowContext& window_context,
               int num_frames_in_flight)
    : context_{window_context.basic_context()} {
  using common::file::GetResourcePath;
  using common::file::GetVkShaderPath;
  using TextureType = ModelBuilder::TextureType;

  const auto original_aspect_ratio =
      util::GetAspectRatio(window_context.frame_size());

  /* Camera */
  common::Camera::Config config;
  config.position = glm::vec3{0.0f, 0.0f, 3.0f};
  camera_ = absl::make_unique<common::UserControlledCamera>(
      config, common::UserControlledCamera::ControlConfig{},
      original_aspect_ratio);
  camera_->SetActivity(true);

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
      context_, "earth", num_frames_in_flight, original_aspect_ratio,
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
      context_, "skybox", num_frames_in_flight, original_aspect_ratio,
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

void Editor::OnEnter(common::Window* mutable_window) {
  (*mutable_window)
      .RegisterScrollCallback([this](double x_pos, double y_pos) {
        camera_->DidScroll(y_pos, 10.0f, 60.0f);
      })
      .RegisterMouseButtonCallback([this](bool is_left, bool is_press) {
        is_pressing_left_ = is_press;
      });
}

void Editor::OnExit(common::Window* mutable_window) {
  (*mutable_window)
      .RegisterScrollCallback(nullptr)
      .RegisterMouseButtonCallback(nullptr);
}

void Editor::Recreate(const wrapper::vulkan::WindowContext& window_context) {
  /* Camera */
  camera_->SetCursorPos(window_context.window().GetCursorPos());

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

void Editor::UpdateData(const common::Window& window, int frame) {
  absl::optional<glm::dvec2> click_ndc;
  if (is_pressing_left_) {
    click_ndc = window.GetCursorPosInNdc();
  }
  earth_.Update(*this, click_ndc);

  const glm::mat4& proj = camera_->projection();
  const glm::mat4& view = camera_->view();
  uniform_buffer_->HostData<EarthTrans>(frame)->proj_view_model =
      proj * view * earth_.model_matrix();
  uniform_buffer_->Flush(frame);

  earth_constant_->HostData<TextureIndex>(frame)->value =
      is_day_ ? kEarthDayTextureIndex : kEarthNightTextureIndex;
  *skybox_constant_->HostData<SkyboxTrans>(frame) =
      {proj, view * earth_.model_matrix()};
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

absl::optional<glm::vec3> Editor::GetIntersectionWithSphere(
    const glm::vec2& click_ndc, float sphere_radius) const {
  // All computation will be done in the object space.
  const glm::mat4 world_to_object = glm::inverse(earth_.model_matrix());
  const glm::mat4 world_to_ndc = camera_->projection() * camera_->view();
  const glm::mat4 ndc_to_world = glm::inverse(world_to_ndc);
  const glm::mat4 ndc_to_object = world_to_object * ndc_to_world;

  const glm::vec3 camera_pos =
      TransformPoint(world_to_object, camera_->position());
  // Z value does not matter since this is in the NDC space.
  const glm::vec3 click_pos =
      TransformPoint(ndc_to_object, glm::vec3{click_ndc, 1.0f});

  glm::vec3 position, normal;
  if (glm::intersectRaySphere(
          camera_pos, glm::normalize(click_pos - camera_pos),
          /*sphereCenter=*/glm::vec3{0.0f}, sphere_radius, position, normal)) {
    return position;
  } else {
    return absl::nullopt;
  }
}

Editor::EarthManager::EarthManager() {
  // Initially, the north pole points to the center of frame.
  model_matrix_ = glm::mat4{kEarthModelRadius};
  model_matrix_ = glm::rotate(model_matrix_, glm::radians(-90.0f),
                              glm::vec3(1.0f, 0.0f, 0.0f));
  model_matrix_ = glm::rotate(model_matrix_, glm::radians(-90.0f),
                              glm::vec3(0.0f, 1.0f, 0.0f));
}

void Editor::EarthManager::Update(
    const Editor& editor, const absl::optional<glm::vec2>& click_ndc) {
  const auto intersection = click_ndc.has_value()
      ? editor.GetIntersectionWithSphere(click_ndc.value(), kEarthModelRadius)
      : absl::nullopt;
  const auto rotation = rotation_manager_.Compute(intersection);
  if (rotation.has_value()) {
    model_matrix_ = glm::rotate(model_matrix_, rotation->angle, rotation->axis);
  }
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
