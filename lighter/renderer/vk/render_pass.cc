//
//  render_pass.cc
//
//  Created by Pujun Lun on 1/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/render_pass.h"

#include "lighter/common/util.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk {
namespace {

using ir::RenderPassDescriptor;

// TODO: Populate image layout.
std::vector<intl::AttachmentDescription> CreateAttachmentDescriptions(
    const RenderPassDescriptor& descriptor,
    absl::Span<const DeviceImage* const> color_attachments,
    absl::Span<const DeviceImage* const> depth_stencil_attachments) {
  std::vector<intl::AttachmentDescription> descriptions;
  descriptions.reserve(color_attachments.size() +
                       depth_stencil_attachments.size());

  for (const DeviceImage* attachment : color_attachments) {
    if (const auto iter = descriptor.color_ops_map.find(attachment);
        iter != descriptor.color_ops_map.end()) {
      const auto& load_store_ops = iter->second;
      descriptions.push_back(intl::AttachmentDescription{}
          .setFormat(attachment->format())
          .setSamples(attachment->sample_count())
          .setLoadOp(type::ConvertAttachmentLoadOp(load_store_ops.load_op))
          .setStoreOp(type::ConvertAttachmentStoreOp(load_store_ops.store_op))
          .setStencilLoadOp(intl::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(intl::AttachmentStoreOp::eDontCare)
          .setInitialLayout(intl::ImageLayout::eGeneral)
          .setFinalLayout(intl::ImageLayout::eGeneral)
      );
    } else {
      FATAL(absl::StrFormat(
          "No color load store ops specified for attachment %s",
          attachment->name()));
    }
  }

  for (const DeviceImage* attachment : depth_stencil_attachments) {
    if (const auto iter = descriptor.depth_stencil_ops_map.find(attachment);
        iter != descriptor.depth_stencil_ops_map.end()) {
      const auto& load_store_ops = iter->second;
      descriptions.push_back(intl::AttachmentDescription{}
          .setFormat(attachment->format())
          .setSamples(attachment->sample_count())
          .setLoadOp(type::ConvertAttachmentLoadOp(
              load_store_ops.depth_ops.load_op))
          .setStoreOp(type::ConvertAttachmentStoreOp(
              load_store_ops.depth_ops.store_op))
          .setStencilLoadOp(type::ConvertAttachmentLoadOp(
              load_store_ops.stencil_ops.load_op))
          .setStencilStoreOp(type::ConvertAttachmentStoreOp(
              load_store_ops.stencil_ops.store_op))
          .setInitialLayout(intl::ImageLayout::eGeneral)
          .setFinalLayout(intl::ImageLayout::eGeneral)
      );
    } else {
      FATAL(absl::StrFormat(
          "No depth stencil load store ops specified for attachment %s",
          attachment->name()));
    }
  }

  return descriptions;
}

std::vector<intl::SubpassDescription> CreateSubpassDescriptions() {
  FATAL("Not yet implemented");
}

std::vector<intl::SubpassDependency> CreateSubpassDependencies() {
  FATAL("Not yet implemented");
}

std::vector<intl::Framebuffer> CreateFrameBuffers() {
  FATAL("Not yet implemented");
}

}  // namespace

RenderPass::RenderPass(const SharedContext& context,
                       const RenderPassDescriptor& descriptor)
    : WithSharedContext{context} {
  // TODO: Only one pipeline is supported for now.
  const auto& pipeline_descriptor =
      descriptor.graphics_ops.value().GetPipeline(0);
  std::vector<const DeviceImage*> attachments;

  for (const auto& [attachment, _] :
           pipeline_descriptor.color_attachment_info_map) {
    attachments.push_back(&DeviceImage::Cast(*attachment));
  }
  const absl::Span<const DeviceImage* const> color_attachments = attachments;

  absl::Span<const DeviceImage* const> depth_stencil_attachments;
  if (pipeline_descriptor.depth_stencil_attachment != nullptr) {
    attachments.push_back(
        &DeviceImage::Cast(*pipeline_descriptor.depth_stencil_attachment));
    depth_stencil_attachments = absl::MakeSpan(&attachments.back(), 1);
  }

  const auto attachment_descriptions = CreateAttachmentDescriptions(
      descriptor, color_attachments, depth_stencil_attachments);
  const auto subpass_descriptions = CreateSubpassDescriptions();
  const auto subpass_dependencies = CreateSubpassDependencies();

  const auto render_pass_create_info = intl::RenderPassCreateInfo{}
      .setAttachments(attachment_descriptions)
      .setSubpasses(subpass_descriptions)
      .setDependencies(subpass_dependencies);
  render_pass_ = context_->device()->createRenderPass(
      render_pass_create_info, *context_->host_allocator());
  framebuffers_ = CreateFrameBuffers();
}

RenderPass::~RenderPass() {
  for (const auto& framebuffer : framebuffers_) {
    context_->device()->destroy(framebuffer, *context_->host_allocator());
  }
  context_->device()->destroy(render_pass_, *context_->host_allocator());
#ifndef NDEBUG
  LOG_INFO << "Render pass destructed";
#endif  // DEBUG
}

}  // namespace lighter::renderer::vk
