//
//  util.cc
//
//  Created by Pujun Lun on 6/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/util.h"

ABSL_FLAG(bool, performance_mode, false,
          "Ignore VSync and present images to the screen as fast as possible");

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;

constexpr uint32_t kVertexBufferBindingPoint = 0;
constexpr uint32_t kImageBindingPoint = 0;

} /* namespace */

ImageViewer::ImageViewer(const SharedBasicContext& context,
                         const SamplableImage& image,
                         int num_channels, bool flip_y) {
  using common::Vertex2D;

  /* Descriptor */
  descriptor_ = absl::make_unique<StaticDescriptor>(
      context, /*infos=*/std::vector<Descriptor::Info>{
          Descriptor::Info{
              Image::GetDescriptorTypeForSampling(),
              VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      kImageBindingPoint,
                      /*array_length=*/1,
                  }},
          }});
  descriptor_->UpdateImageInfos(
      Image::GetDescriptorTypeForSampling(),
      /*image_info_map=*/
      {{kImageBindingPoint, {image.GetDescriptorInfoForSampling()}}});

  /* Vertex buffer */
  const auto vertex_data = Vertex2D::GetFullScreenSquadVertices(flip_y);
  const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
      /*per_mesh_vertices=*/{{PerVertexBuffer::VertexDataInfo{vertex_data}}}
  };
  vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context, vertex_data_info, pipeline::GetVertexAttribute<Vertex2D>());

  /* Pipeline */
  const auto frag_shader_relative_path =
      num_channels == common::kBwImageChannel
          ? "image_viewer/view_bw_image.frag"
          : "image_viewer/view_color_image.frag";
  pipeline_builder_ = absl::make_unique<GraphicsPipelineBuilder>(context);
  (*pipeline_builder_)
      .SetPipelineName("View image")
      .AddVertexInput(kVertexBufferBindingPoint,
                      pipeline::GetPerVertexBindingDescription<Vertex2D>(),
                      vertex_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({descriptor_->layout()}, /*push_constant_ranges=*/{})
      .SetColorBlend(
          {pipeline::GetColorAlphaBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("image_viewer/view_image.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath(frag_shader_relative_path));
}

void ImageViewer::UpdateFramebuffer(const VkExtent2D& frame_size,
                                    const RenderPass& render_pass,
                                    uint32_t subpass_index) {
  (*pipeline_builder_)
      .SetViewport(pipeline::GetFullFrameViewport(frame_size))
      .SetRenderPass(*render_pass, subpass_index);
  pipeline_ = pipeline_builder_->Build();
}

void ImageViewer::Draw(const VkCommandBuffer& command_buffer) const {
  pipeline_->Bind(command_buffer);
  descriptor_->Bind(command_buffer, pipeline_->layout(),
                    pipeline_->binding_point());
  vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                       /*mesh_index=*/0, /*instance_count=*/1);
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
