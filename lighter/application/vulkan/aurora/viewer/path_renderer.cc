//
//  path_renderer.cc
//
//  Created by Pujun Lun on 4/18/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/aurora/viewer/path_renderer.h"

#include "lighter/renderer/vulkan/extension/align.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"
#include "lighter/renderer/vulkan/wrapper/render_pass_util.h"
#include "lighter/renderer/vulkan/wrapper/util.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace renderer::vulkan;

enum SubpassIndex {
  kDumpPathsSubpassIndex = 0,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kDumpPathsSubpassIndex,
};

enum ImageBindingPoint {
  kOriginalImageBindingPoint = 0,
  kOutputImageBindingPoint,
};

constexpr uint32_t kVertexBufferBindingPoint = 0;

/* BEGIN: Consistent with work group size defined in shaders. */

constexpr uint32_t kWorkGroupSizeX = 16;
constexpr uint32_t kWorkGroupSizeY = 16;

/* END: Consistent with work group size defined in shaders. */

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Transformation {
  ALIGN_MAT4 glm::mat4 proj_view;
};

/* END: Consistent with uniform blocks defined in shaders. */

} /* namespace */

PathRenderer2D::PathRenderer2D(
    const SharedBasicContext& context,
    const OffscreenImage& intermediate_image,
    const OffscreenImage& output_image,
    MultisampleImage::Mode multisampling_mode,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : work_group_count_{util::GetWorkGroupCount(
          intermediate_image.extent(), {kWorkGroupSizeX, kWorkGroupSizeY})},
      aurora_paths_vertex_buffers_{std::move(aurora_paths_vertex_buffers)} {
  const auto& image_extent = intermediate_image.extent();
  ASSERT_TRUE(output_image.extent().width == image_extent.width &&
                  output_image.extent().height == image_extent.height,
              "Size of intermediate_image and output images must match");

  /* Image */
  multisample_image_ = MultisampleImage::CreateColorMultisampleImage(
      context, intermediate_image, multisampling_mode);

  /* Push constant */
  trans_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(Transformation), /*num_frames_in_flight=*/1);

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      kNumOverlaySubpasses,
  };
  NaiveRenderPassBuilder render_pass_builder{
      context, subpass_config, /*num_framebuffers=*/1,
      /*use_multisampling=*/true,
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kSampledAsTexture,
  };
  render_pass_builder
      .UpdateAttachmentImage(
          render_pass_builder.color_attachment_index(),
          [&intermediate_image](int) -> const Image& {
            return intermediate_image;
          })
      .UpdateAttachmentImage(
          render_pass_builder.multisample_attachment_index(),
          [this](int) -> const Image& { return *multisample_image_; });
  render_pass_ = render_pass_builder.Build();
  render_op_ = [this](const VkCommandBuffer& command_buffer) {
    for (const auto* buffer : aurora_paths_vertex_buffers_) {
      buffer->Draw(command_buffer, kVertexBufferBindingPoint,
                   /*mesh_index=*/0, /*instance_count=*/1);
    }
  };

  /* Descriptor */
  bold_paths_descriptor_ = absl::make_unique<StaticDescriptor>(
      context, std::vector<Descriptor::Info>{
          Descriptor::Info{
              Image::GetDescriptorTypeForLinearAccess(),
              VK_SHADER_STAGE_COMPUTE_BIT,
              /*bindings=*/{
                  {kOriginalImageBindingPoint, /*array_length=*/1},
                  {kOutputImageBindingPoint, /*array_length=*/1},
              },
          },
      });
  bold_paths_descriptor_->UpdateImageInfos(
      Image::GetDescriptorTypeForLinearAccess(),
      /*image_info_map=*/{
          {kOriginalImageBindingPoint,
           {intermediate_image.GetDescriptorInfoForLinearAccess()}},
          {kOutputImageBindingPoint,
           {output_image.GetDescriptorInfoForLinearAccess()}},
      });

  /* Pipeline */
  render_paths_pipeline_ = GraphicsPipelineBuilder{context}
      .SetPipelineName("Dump path")
      .SetMultisampling(multisample_image_->sample_count())
      .SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
      .AddVertexInput(
          kVertexBufferBindingPoint,
          pipeline::GetPerVertexBindingDescription<common::Vertex3DPosOnly>(),
          aurora_paths_vertex_buffers_[0]->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout(/*descriptor_layouts=*/{},
                         {trans_constant_->MakePerFrameRange(
                             VK_SHADER_STAGE_VERTEX_BIT)})
      .SetViewport(pipeline::GetFullFrameViewport(image_extent),
                   /*flip_y=*/false)
      .SetRenderPass(**render_pass_, kDumpPathsSubpassIndex)
      .SetColorBlend(
          {pipeline::GetColorAlphaBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("aurora/dump_path.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("aurora/dump_path.frag"))
      .Build();

  bold_paths_pipeline_ = ComputePipelineBuilder{context}
      .SetPipelineName("Bold paths")
      .SetPipelineLayout({bold_paths_descriptor_->layout()},
                         /*push_constant_ranges=*/{})
      .SetShader(common::file::GetVkShaderPath("aurora/bold_path.comp"))
      .Build();
}

void PathRenderer2D::RenderPaths(const VkCommandBuffer& command_buffer,
                                 const common::Camera& camera) {
  trans_constant_->HostData<Transformation>(/*frame=*/0)->proj_view =
      camera.GetProjectionMatrix() * camera.GetViewMatrix();
  render_paths_pipeline_->Bind(command_buffer);
  trans_constant_->Flush(command_buffer, render_paths_pipeline_->layout(),
                         /*frame=*/0, /*target_offset=*/0,
                         VK_SHADER_STAGE_VERTEX_BIT);
  render_pass_->Run(command_buffer, /*framebuffer_index=*/0,
                    absl::MakeSpan(&render_op_, 1));
}

void PathRenderer2D::BoldPaths(const VkCommandBuffer& command_buffer) {
  bold_paths_pipeline_->Bind(command_buffer);
  bold_paths_descriptor_->Bind(command_buffer, bold_paths_pipeline_->layout(),
                               bold_paths_pipeline_->binding_point());
  vkCmdDispatch(command_buffer, work_group_count_.width,
                work_group_count_.height, /*groupCountZ=*/1);
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
