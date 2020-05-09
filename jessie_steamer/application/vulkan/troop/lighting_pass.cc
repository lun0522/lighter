//
//  lighting_pass.cc
//
//  Created by Pujun Lun on 5/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/troop/lighting_pass.h"

#include <array>
#include <algorithm>
#include <cstring>
#include <random>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace troop {
namespace {

using namespace wrapper::vulkan;

enum SubpassIndex {
  kLightsSubpassIndex = 0,
  kSoldiersSubpassIndex,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kSoldiersSubpassIndex,
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
constexpr int kNumLights = 32;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Lights {
  ALIGN_VEC4 glm::vec4 colors[kNumLights];
};

struct RenderInfo {
  ALIGN_VEC4 glm::vec4 light_centers[kNumLights];
  ALIGN_VEC4 glm::vec4 camera_pos;
};

/* END: Consistent with uniform blocks defined in shaders. */

} /* namespace */

LightingPass::LightingPass(const WindowContext* window_context,
                           int num_frames_in_flight)
    : window_context_{*FATAL_IF_NULL(window_context)} {
  using common::Vertex2D;
  const auto context = window_context_.basic_context();

  /* Uniform buffer */
  lights_uniform_ = absl::make_unique<UniformBuffer>(
      context, sizeof(Lights), /*num_chunks=*/1);
  render_info_uniform_ = absl::make_unique<UniformBuffer>(
      context, sizeof(RenderInfo), num_frames_in_flight);

  std::random_device device;
  std::mt19937 rand_gen{device()};
  std::uniform_real_distribution<float> color{0.5f, 1.0f};
  std::uniform_real_distribution<float> center_x{0.0f, 6.8f};
  std::uniform_real_distribution<float> center_z{0.0f, 9.0f};

  auto& light_colors =
      lights_uniform_->HostData<Lights>(/*chunk_index=*/0)->colors;
  std::generate(
      light_colors, light_colors + kNumLights,
      [&rand_gen, &color]() -> glm::vec4 {
        return {color(rand_gen), color(rand_gen), color(rand_gen), 0.0f};
      });
  lights_uniform_->Flush(/*chunk_index=*/0);

  original_light_centers_.resize(kNumLights);
  std::generate(
      original_light_centers_.begin(), original_light_centers_.end(),
      [&rand_gen, &center_x, &center_z]() -> glm::vec4 {
        return {center_x(rand_gen), 3.5f, center_z(rand_gen), 0.0f};
      });

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

  const std::array<Descriptor::Info, 2> descriptor_infos{
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

  descriptors_.resize(num_frames_in_flight);
  for (int frame = 0; frame < num_frames_in_flight; ++frame) {
    descriptors_[frame] =
        absl::make_unique<StaticDescriptor>(context, descriptor_infos);
    descriptors_[frame]->UpdateBufferInfos(
        UniformBuffer::GetDescriptorType(),
        /*buffer_info_map=*/{
            {kLightsUniformBufferBindingPoint,
                {lights_uniform_->GetDescriptorInfo(/*chunk_index=*/0)}},
            {kRenderInfoUniformBufferBindingPoint,
                {render_info_uniform_->GetDescriptorInfo(frame)}},
        });
  }

  /* Vertex buffer */
  const auto vertex_data =
      Vertex2D::GetFullScreenSquadVertices(/*flip_y=*/false);
  const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
      /*per_mesh_vertices=*/{{PerVertexBuffer::VertexDataInfo{vertex_data}}}
  };
  squad_vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context, vertex_data_info, pipeline::GetVertexAttribute<Vertex2D>());

  /* Pipeline */
  soldiers_pipeline_builder_ =
      absl::make_unique<GraphicsPipelineBuilder>(context);
  (*soldiers_pipeline_builder_)
      .SetPipelineName("Soldiers")
      .AddVertexInput(kVertexBufferBindingPoint,
                      pipeline::GetPerVertexBindingDescription<Vertex2D>(),
                      squad_vertex_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({descriptors_[0]->layout()},
                         /*push_constant_ranges=*/{})
      .SetColorBlend(
          {pipeline::GetColorAlphaBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("troop/lighting_pass.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("troop/lighting_pass.frag"));

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/true,
      /*num_transparent_subpasses=*/0,
      kNumOverlaySubpasses,
  };
  render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
      context, subpass_config,
      /*num_framebuffers=*/window_context_.num_swapchain_images(),
      /*use_multisampling=*/false,
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kPresentToScreen,
      /*preserve_depth_attachment_content=*/true);
}

void LightingPass::UpdateFramebuffer(
    const Image& depth_stencil_image,
    const OffscreenImage& position_image,
    const OffscreenImage& normal_image,
    const OffscreenImage& diffuse_specular_image) {
  /* Descriptor */
  for (auto& descriptor : descriptors_) {
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
  (*render_pass_builder_)
      .UpdateAttachmentImage(
          render_pass_builder_->color_attachment_index(),
          [this](int framebuffer_index) -> const Image& {
            return window_context_.swapchain_image(framebuffer_index);
          })
      .UpdateAttachmentImage(
          render_pass_builder_->depth_attachment_index(),
          [&depth_stencil_image](int framebuffer_index) -> const Image& {
            return depth_stencil_image;
          });
  render_pass_ = render_pass_builder_->Build();

  /* Pipeline */
  (*soldiers_pipeline_builder_)
      .SetViewport(pipeline::GetFullFrameViewport(window_context_.frame_size()))
      .SetRenderPass(**render_pass_, kSoldiersSubpassIndex);
  soldiers_pipeline_ = soldiers_pipeline_builder_->Build();
}

void LightingPass::UpdatePerFrameData(int frame, const common::Camera& camera) {
  auto& render_info = *render_info_uniform_->HostData<RenderInfo>(frame);
  render_info.camera_pos = glm::vec4{camera.position(), 0.0f};
  // TODO
  const glm::vec2 pos_increment = glm::vec2{0.0f, 3.0f} *
                                  timer_.GetElapsedTimeSinceLaunch();
  const glm::vec2 bound = glm::vec2{6.8f, 9.0f};
  auto& centers = render_info.light_centers;
  for (int light = 0; light < kNumLights; ++light) {
    centers[light].x =
        glm::mod(original_light_centers_[light].x + pos_increment.x, bound.x);
    centers[light].y = original_light_centers_[light].y;
    centers[light].z =
        -glm::mod(original_light_centers_[light].z + pos_increment.y, bound.y);
  }
  render_info_uniform_->Flush(frame);
}

void LightingPass::Draw(const VkCommandBuffer& command_buffer,
                        uint32_t framebuffer_index, int current_frame) const {
  render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
      [](const VkCommandBuffer& command_buffer) {

      },
      [this, current_frame](const VkCommandBuffer& command_buffer) {
        soldiers_pipeline_->Bind(command_buffer);
        descriptors_[current_frame]->Bind(
            command_buffer, soldiers_pipeline_->layout(),
            soldiers_pipeline_->binding_point());
        squad_vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                                   /*mesh_index=*/0, /*instance_count=*/1);
      },
  });
}

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
