//
//  lighting_pass.cc
//
//  Created by Pujun Lun on 5/5/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/troop/lighting_pass.h"

#include <array>
#include <algorithm>
#include <cstring>
#include <random>

#include "lighter/common/file.h"
#include "lighter/renderer/common/align.h"
#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "lighter/renderer/vulkan/extension/naive_render_pass.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/glm/glm.hpp"
#include "third_party/glm/gtc/matrix_transform.hpp"

namespace lighter {
namespace application {
namespace vulkan {
namespace troop {
namespace {

using namespace renderer::vulkan;

enum SubpassIndex {
  kLightsSubpassIndex = 0,
  kSoldiersSubpassIndex,
  kNumSubpasses,
};

enum UniformBindingPoint {
  kLightsUniformBufferBindingPoint = 0,
  kRenderInfoUniformBufferBindingPoint,
  kPositionTextureBindingPoint,
  kNormalTextureBindingPoint,
  kDiffuseSpecularTextureBindingPoint,
  kNumUniforms,
  kNumUniformBuffers = kPositionTextureBindingPoint -
                       kLightsUniformBufferBindingPoint,
  kNumTextures = kNumUniforms - kPositionTextureBindingPoint,
};

constexpr uint32_t kVertexBufferBindingPoint = 0;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

constexpr int kNumLights = 32;

struct Lights {
  ALIGN_VEC4 glm::vec4 colors[kNumLights];
};

struct RenderInfo {
  ALIGN_VEC4 glm::vec4 light_centers[kNumLights];
  ALIGN_VEC4 glm::vec4 camera_pos;
};

struct Transformation {
  ALIGN_MAT4 glm::mat4 model;
  ALIGN_MAT4 glm::mat4 proj_view;
};

/* END: Consistent with uniform blocks defined in shaders. */

// Generates original centers for lights.
std::vector<glm::vec3> GenerateOriginalLightCenters(
    const LightingPass::LightCenterConfig& config) {
  std::random_device device;
  std::mt19937 rand_gen{device()};
  std::uniform_real_distribution<float> center_x{config.bound_x.x,
                                                 config.bound_x.y};
  std::uniform_real_distribution<float> center_y{config.bound_y.x,
                                                 config.bound_y.y};
  std::uniform_real_distribution<float> center_z{config.bound_z.x,
                                                 config.bound_z.y};

  std::vector<glm::vec3> centers(kNumLights);
  std::generate(centers.begin(), centers.end(),
                [&rand_gen, &center_x, &center_y, &center_z]() {
                  return glm::vec4{center_x(rand_gen), center_y(rand_gen),
                                   center_z(rand_gen), 0.0f};
                });
  return centers;
}

// Wraps 'coord' around to make it fall into the range ['bound.x', 'bound.y').
// We assume that 'bound.x' <= 'bound.y'.
float WrapAroundCoordinate(float coord, const glm::vec2& bound) {
  ASSERT_TRUE(bound.x <= bound.y,
              "bound.x and bound.y should be lower and upper bound");
  if (bound.x == bound.y) {
    return bound.x;
  }

  if (coord < bound.x) {
    return bound.y - glm::mod(coord - bound.x, bound.y - bound.x);
  } else if (coord >= bound.y) {
    return bound.x + glm::mod(coord - bound.y, bound.y - bound.x);
  } else {
    return coord;
  }
}

// Wraps around light centers in 'render_info' to make them fall into the bounds
// specified in 'config'.
void WrapAroundLightCenters(const LightingPass::LightCenterConfig& config,
                            RenderInfo* render_info) {
  for (auto& center : render_info->light_centers) {
    center.x = WrapAroundCoordinate(center.x, config.bound_x);
    center.y = WrapAroundCoordinate(center.y, config.bound_y);
    center.z = WrapAroundCoordinate(center.z, config.bound_z);
  }
}

} /* namespace */

LightingPass::LightingPass(const WindowContext* window_context,
                           int num_frames_in_flight,
                           const LightCenterConfig& config)
    : light_center_config_{config},
      original_light_centers_{GenerateOriginalLightCenters(config)},
      window_context_{*FATAL_IF_NULL(window_context)} {
  using common::Vertex2D;
  using common::Vertex3DPosOnly;
  const auto context = window_context_.basic_context();

  /* Uniform buffer and push constant */
  lights_colors_uniform_ = absl::make_unique<UniformBuffer>(
      context, sizeof(Lights), /*num_chunks=*/1);
  render_info_uniform_ = absl::make_unique<UniformBuffer>(
      context, sizeof(RenderInfo), num_frames_in_flight);
  lights_trans_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(Transformation), num_frames_in_flight);

  std::random_device device;
  std::mt19937 rand_gen{device()};
  std::uniform_real_distribution<float> color{0.5f, 1.0f};
  std::uniform_real_distribution<float> center_x{0.0f, 6.8f};
  std::uniform_real_distribution<float> center_z{0.0f, 9.0f};

  auto& light_colors =
      lights_colors_uniform_->HostData<Lights>(/*chunk_index=*/0)->colors;
  std::generate(
      light_colors, light_colors + kNumLights,
      [&rand_gen, &color]() -> glm::vec4 {
        return {color(rand_gen), color(rand_gen), color(rand_gen), 0.0f};
      });
  lights_colors_uniform_->Flush(/*chunk_index=*/0);

  /* Descriptor */
  std::vector<Descriptor::Info::Binding> uniform_buffer_bindings;
  uniform_buffer_bindings.reserve(kNumUniformBuffers);
  for (uint32_t i = kLightsUniformBufferBindingPoint;
       i <= kRenderInfoUniformBufferBindingPoint; ++i) {
    uniform_buffer_bindings.push_back(Descriptor::Info::Binding{
        /*binding_point=*/i,
        /*array_length=*/1,
    });
  }

  std::vector<Descriptor::Info::Binding> texture_bindings;
  texture_bindings.reserve(kNumTextures);
  for (uint32_t i = kPositionTextureBindingPoint;
       i <= kDiffuseSpecularTextureBindingPoint; ++i) {
    texture_bindings.push_back(Descriptor::Info::Binding{
        /*binding_point=*/i,
        /*array_length=*/1,
    });
  }

  const Descriptor::Info lights_descriptor_info{
      Descriptor::Info{
          UniformBuffer::GetDescriptorType(),
          VK_SHADER_STAGE_VERTEX_BIT,
          uniform_buffer_bindings,
      },
  };

  const std::array<Descriptor::Info, 2> soldiers_descriptor_infos{
      Descriptor::Info{
          UniformBuffer::GetDescriptorType(),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          uniform_buffer_bindings,
      },
      Descriptor::Info{
          Image::GetDescriptorTypeForSampling(),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          texture_bindings,
      },
  };

  lights_descriptors_.resize(num_frames_in_flight);
  soldiers_descriptors_.resize(num_frames_in_flight);
  for (int frame = 0; frame < num_frames_in_flight; ++frame) {
    const Descriptor::BufferInfoMap buffer_info_map{
        {kLightsUniformBufferBindingPoint,
            {lights_colors_uniform_->GetDescriptorInfo(/*chunk_index=*/0)}},
        {kRenderInfoUniformBufferBindingPoint,
            {render_info_uniform_->GetDescriptorInfo(frame)}},
    };

    lights_descriptors_[frame] = absl::make_unique<StaticDescriptor>(
        context, absl::MakeSpan(&lights_descriptor_info, 1));
    lights_descriptors_[frame]->UpdateBufferInfos(
        UniformBuffer::GetDescriptorType(), buffer_info_map);

    soldiers_descriptors_[frame] =
        absl::make_unique<StaticDescriptor>(context, soldiers_descriptor_infos);
    soldiers_descriptors_[frame]->UpdateBufferInfos(
        UniformBuffer::GetDescriptorType(), buffer_info_map);
  }

  /* Vertex buffer */
  const common::ObjFilePosOnly cube_file{
      common::file::GetResourcePath("model/cube.obj"), /*index_base=*/1};
  PerVertexBuffer::NoShareIndicesDataInfo cube_vertex_data_info{
      /*per_mesh_infos=*/{{
          PerVertexBuffer::VertexDataInfo{cube_file.indices},
          PerVertexBuffer::VertexDataInfo{cube_file.vertices},
      }},
  };
  cube_vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context, std::move(cube_vertex_data_info),
      pipeline::GetVertexAttributes<Vertex3DPosOnly>());

  // Since we did flip the viewport in the geometry pass (because the depth
  // stencil image is reused in this pass), we need to flip Y coordinate here.
  const auto squad_vertex_data =
      Vertex2D::GetFullScreenSquadVertices(/*flip_y=*/true);
  PerVertexBuffer::NoIndicesDataInfo squad_vertex_data_info{
      /*per_mesh_vertices=*/
      {{PerVertexBuffer::VertexDataInfo{squad_vertex_data}}}
  };
  squad_vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context, std::move(squad_vertex_data_info),
      pipeline::GetVertexAttributes<Vertex2D>());

  /* Pipeline */
  constexpr uint32_t kStencilReference = 0xFF;
  lights_pipeline_builder_ =
      absl::make_unique<GraphicsPipelineBuilder>(context);
  (*lights_pipeline_builder_)
      .SetPipelineName("Lights")
      .SetDepthTestEnable(/*enable_test=*/true, /*enable_write=*/true)
      .SetStencilTestEnable(true)
      .SetStencilOpState(pipeline::GetStencilWriteOpState(kStencilReference),
                         VK_STENCIL_FACE_FRONT_BIT)
      .AddVertexInput(
          kVertexBufferBindingPoint,
          pipeline::GetPerVertexBindingDescription<Vertex3DPosOnly>(),
          cube_vertex_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({lights_descriptors_[0]->layout()},
                         {lights_trans_constant_->MakePerFrameRange(
                             VK_SHADER_STAGE_VERTEX_BIT)})
      .SetColorBlend(
          {pipeline::GetColorAlphaBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("troop/light_cube.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("troop/light_cube.frag"));

  soldiers_pipeline_builder_ =
      absl::make_unique<GraphicsPipelineBuilder>(context);
  (*soldiers_pipeline_builder_)
      .SetPipelineName("Soldiers")
      .SetStencilTestEnable(true)
      .SetStencilOpState(
          pipeline::GetStencilReadOpState(VK_COMPARE_OP_NOT_EQUAL,
                                          kStencilReference),
          VK_STENCIL_FACE_FRONT_BIT)
      .AddVertexInput(kVertexBufferBindingPoint,
                      pipeline::GetPerVertexBindingDescription<Vertex2D>(),
                      squad_vertex_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({soldiers_descriptors_[0]->layout()},
                         /*push_constant_ranges=*/{})
      .SetColorBlend(
          {pipeline::GetColorAlphaBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("troop/lighting_pass.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("troop/lighting_pass.frag"));
}

void LightingPass::UpdateFramebuffer(
    const Image& depth_stencil_image,
    const OffscreenImage& position_image,
    const OffscreenImage& normal_image,
    const OffscreenImage& diffuse_specular_image) {
  /* Descriptor */
  for (auto& descriptor : soldiers_descriptors_) {
    descriptor->UpdateImageInfos(
        Image::GetDescriptorTypeForSampling(),
        Descriptor::ImageInfoMap{
            {kPositionTextureBindingPoint,
                {position_image.GetDescriptorInfoForSampling()}},
            {kNormalTextureBindingPoint,
                {normal_image.GetDescriptorInfoForSampling()}},
            {kDiffuseSpecularTextureBindingPoint,
                {diffuse_specular_image.GetDescriptorInfoForSampling()}},
        });
  }

  /* Render pass */
  if (render_pass_builder_ == nullptr) {
    CreateRenderPassBuilder(depth_stencil_image);
  }

  (*render_pass_builder_)
      .UpdateAttachmentImage(
          swapchain_image_info_.index(),
          [this](int framebuffer_index) -> const Image& {
            return window_context_.swapchain_image(framebuffer_index);
          })
      .UpdateAttachmentImage(
          depth_stencil_image_info_.index(),
          [&depth_stencil_image](int framebuffer_index) -> const Image& {
            return depth_stencil_image;
          });
  render_pass_ = render_pass_builder_->Build();

  /* Pipeline */
  const auto viewport =
      pipeline::GetFullFrameViewport(window_context_.frame_size());
  (*lights_pipeline_builder_)
      .SetViewport(viewport)
      .SetRenderPass(**render_pass_, kLightsSubpassIndex);
  lights_pipeline_ = lights_pipeline_builder_->Build();

  (*soldiers_pipeline_builder_)
      .SetViewport(viewport)
      .SetRenderPass(**render_pass_, kSoldiersSubpassIndex);
  soldiers_pipeline_ = soldiers_pipeline_builder_->Build();
}

void LightingPass::UpdatePerFrameData(int frame, const common::Camera& camera,
                                      float light_model_scale) {
  auto& light_trans = *lights_trans_constant_->HostData<Transformation>(frame);
  light_trans.model = glm::scale(glm::mat4{1.0f}, glm::vec3{light_model_scale});
  light_trans.proj_view = camera.GetProjectionMatrix() * camera.GetViewMatrix();

  auto& render_info = *render_info_uniform_->HostData<RenderInfo>(frame);
  render_info.camera_pos = glm::vec4{camera.position(), 0.0f};
  const glm::vec3 light_center_increments = light_center_config_.increments *
                                            timer_.GetElapsedTimeSinceLaunch();
  for (int light = 0; light < kNumLights; ++light) {
    render_info.light_centers[light] =
        {original_light_centers_[light] + light_center_increments, 0.0f};
  }
  WrapAroundLightCenters(light_center_config_, &render_info);
  render_info_uniform_->Flush(frame);
}

void LightingPass::Draw(const VkCommandBuffer& command_buffer,
                        uint32_t framebuffer_index, int current_frame) const {
  render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
      [this, current_frame](const VkCommandBuffer& command_buffer) {
        lights_pipeline_->Bind(command_buffer);
        lights_descriptors_[current_frame]->Bind(
            command_buffer, lights_pipeline_->layout(),
            lights_pipeline_->binding_point());
        lights_trans_constant_->Flush(
            command_buffer, lights_pipeline_->layout(), current_frame,
            /*target_offset=*/0, VK_SHADER_STAGE_VERTEX_BIT);
        cube_vertex_buffer_->Draw(
            command_buffer, kVertexBufferBindingPoint,
            /*mesh_index=*/0, /*instance_count=*/kNumLights);
      },
      [this, current_frame](const VkCommandBuffer& command_buffer) {
        soldiers_pipeline_->Bind(command_buffer);
        soldiers_descriptors_[current_frame]->Bind(
            command_buffer, soldiers_pipeline_->layout(),
            soldiers_pipeline_->binding_point());
        squad_vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                                   /*mesh_index=*/0, /*instance_count=*/1);
      },
  });
}

void LightingPass::CreateRenderPassBuilder(const Image& depth_stencil_image) {
  image::UsageTracker image_usage_tracker;
  swapchain_image_info_.AddToTracker(
      image_usage_tracker, window_context_.swapchain_image(/*index=*/0));
  depth_stencil_image_info_.AddToTracker(image_usage_tracker,
                                         depth_stencil_image);

  const NaiveRenderPass::SubpassConfig subpass_config{
      kNumSubpasses, /*first_transparent_subpass=*/kSoldiersSubpassIndex,
      /*first_overlay_subpass=*/absl::nullopt,
  };

  const auto color_attachment_config =
      swapchain_image_info_.MakeAttachmentConfig()
          .set_final_usage(image::Usage::GetPresentationUsage());

  auto depth_stencil_load_store_ops =
      GraphicsPass::GetDefaultDepthStencilLoadStoreOps();
  depth_stencil_load_store_ops.depth_load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
  const auto depth_stencil_attachment_config =
      depth_stencil_image_info_.MakeAttachmentConfig()
          .set_load_store_ops(depth_stencil_load_store_ops);

  render_pass_builder_ = NaiveRenderPass::CreateBuilder(
      window_context_.basic_context(),
      /*num_framebuffers=*/window_context_.num_swapchain_images(),
      subpass_config, color_attachment_config,
      /*multisampling_attachment_config=*/nullptr,
      &depth_stencil_attachment_config, image_usage_tracker);
}

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
