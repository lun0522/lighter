//
//  util.cc
//
//  Created by Pujun Lun on 6/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/util.h"

ABSL_FLAG(bool, performance_mode, false,
          "Ignore VSync and present images to the screen as fast as possible");

namespace lighter {
namespace application {
namespace vulkan {
namespace {

using namespace renderer::vulkan;

constexpr uint32_t kVertexBufferBindingPoint = 0;
constexpr uint32_t kImageBindingPoint = 0;

} /* namespace */

void SimpleApp::RecreateRenderPass(
    const NaiveRenderPass::SubpassConfig& subpass_config) {
  /* Depth stencil image */
  if (subpass_config.use_depth_stencil()) {
    depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
        context(), window_context().frame_size(),
        window_context().multisampling_mode());
#ifndef NDEBUG
    LOG_INFO << "Depth stencil image created";
#endif /* !NDEBUG */
  }

  if (render_pass_builder_ == nullptr) {
    CreateRenderPassBuilder(subpass_config);
  }

  render_pass_builder_->UpdateAttachmentImage(
      swapchain_image_info_.index(),
      [this](int framebuffer_index) -> const Image& {
        return window_context().swapchain_image(framebuffer_index);
      });
  if (depth_stencil_image_ != nullptr) {
    render_pass_builder_->UpdateAttachmentImage(
        depth_stencil_image_info_.index(),
        [this](int framebuffer_index) -> const Image& {
          return *depth_stencil_image_;
        });
  }
  if (window_context().use_multisampling()) {
    render_pass_builder_->UpdateAttachmentImage(
        multisample_image_info_.index(),
        [this](int framebuffer_index) -> const Image& {
          return window_context().multisample_image();
        });
  }
  render_pass_ = render_pass_builder_->Build();
}

void SimpleApp::CreateRenderPassBuilder(
    const NaiveRenderPass::SubpassConfig& subpass_config) {
  /* Image usage tracker */
  const bool use_depth_stencil = depth_stencil_image_ != nullptr;
  const bool use_multisampling = window_context().use_multisampling();
  image::UsageTracker image_usage_tracker;
  swapchain_image_info_.AddToTracker(
      image_usage_tracker, window_context().swapchain_image(/*index=*/0));
  if (use_depth_stencil) {
    depth_stencil_image_info_.AddToTracker(image_usage_tracker,
                                           *depth_stencil_image_);
  }
  if (use_multisampling) {
    multisample_image_info_.AddToTracker(image_usage_tracker,
                                         window_context().multisample_image());
  }

  /* Render pass builder */
  const auto color_attachment_config =
      NaiveRenderPass::AttachmentConfig{&swapchain_image_info_}
          .set_final_usage(image::Usage::GetPresentationUsage());
  const NaiveRenderPass::AttachmentConfig multisampling_attachment_config{
      &multisample_image_info_};
  const NaiveRenderPass::AttachmentConfig depth_stencil_attachment_config{
      &depth_stencil_image_info_};
  render_pass_builder_ = NaiveRenderPass::CreateBuilder(
      context(), /*num_framebuffers=*/window_context().num_swapchain_images(),
      subpass_config, color_attachment_config,
      use_multisampling ? &multisampling_attachment_config : nullptr,
      use_depth_stencil ? &depth_stencil_attachment_config : nullptr,
      image_usage_tracker);
}

ImageViewer::ImageViewer(const SharedBasicContext& context,
                         const SamplableImage& image,
                         int num_channels, bool flip_y) {
  using common::Vertex2D;

  /* Descriptor */
  descriptor_ = absl::make_unique<StaticDescriptor>(
      context, std::vector<Descriptor::Info>{
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
} /* namespace lighter */
