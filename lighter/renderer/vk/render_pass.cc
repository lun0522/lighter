//
//  render_pass.cc
//
//  Created by Pujun Lun on 1/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/render_pass.h"

#include "lighter/common/util.h"
#include "lighter/renderer/pipeline.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk {
namespace {

// TODO: Populate image layout.
std::vector<VkAttachmentDescription> CreateAttachmentDescriptions(
    const RenderPassDescriptor& descriptor,
    absl::Span<const DeviceImage* const> color_attachments,
    absl::Span<const DeviceImage* const> depth_stencil_attachments) {
  std::vector<VkAttachmentDescription> descriptions;
  descriptions.reserve(color_attachments.size() +
                       depth_stencil_attachments.size());

  for (const DeviceImage* attachment : color_attachments) {
    if (const auto iter = descriptor.color_ops_map.find(attachment);
        iter != descriptor.color_ops_map.end()) {
      const auto& load_store_ops = iter->second;
      descriptions.push_back({
          .flags = nullflag,
          attachment->format(),
          attachment->sample_count(),
          type::ConvertAttachmentLoadOp(load_store_ops.load_op),
          type::ConvertAttachmentStoreOp(load_store_ops.store_op),
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
          .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
         });
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
      descriptions.push_back({
          .flags = nullflag,
          attachment->format(),
          attachment->sample_count(),
          type::ConvertAttachmentLoadOp(load_store_ops.depth_ops.load_op),
          type::ConvertAttachmentStoreOp(load_store_ops.depth_ops.store_op),
          type::ConvertAttachmentLoadOp(load_store_ops.stencil_ops.load_op),
          type::ConvertAttachmentStoreOp(load_store_ops.stencil_ops.store_op),
          .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
          .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
      });
    } else {
      FATAL(absl::StrFormat(
          "No depth stencil load store ops specified for attachment %s",
          attachment->name()));
    }
  }

  return descriptions;
}

std::vector<VkSubpassDescription> CreateSubpassDescriptions() {
  FATAL("Not yet implemented");
}

std::vector<VkSubpassDependency> CreateSubpassDependencies() {
  FATAL("Not yet implemented");
}

std::vector<VkFramebuffer> CreateFrameBuffers() {
  FATAL("Not yet implemented");
}

}  // namespace

RenderPass::RenderPass(SharedContext context,
                       const RenderPassDescriptor& descriptor)
    : context_{std::move(context)} {
  // TODO: Only one pipeline is supported for now.
  const auto& pipeline_descriptor =
      descriptor.graphics_ops.value().GetPipeline(0);
  std::vector<const DeviceImage*> attachments;

  for (const auto& pair : pipeline_descriptor.color_attachment_info_map) {
    attachments.push_back(&DeviceImage::Cast(*pair.first));
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

  const VkRenderPassCreateInfo render_pass_info{
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      CONTAINER_SIZE(attachment_descriptions),
      attachment_descriptions.data(),
      CONTAINER_SIZE(subpass_descriptions),
      subpass_descriptions.data(),
      CONTAINER_SIZE(subpass_dependencies),
      subpass_dependencies.data(),
  };

  ASSERT_SUCCESS(vkCreateRenderPass(*context_->device(), &render_pass_info,
                                    *context_->host_allocator(), &render_pass_),
                 "Failed to create render pass");

  framebuffers_ = CreateFrameBuffers();
}

RenderPass::~RenderPass() {
  for (const auto& framebuffer : framebuffers_) {
    vkDestroyFramebuffer(*context_->device(), framebuffer,
                         *context_->host_allocator());
  }
  vkDestroyRenderPass(*context_->device(), render_pass_,
                      *context_->host_allocator());
#ifndef NDEBUG
  LOG_INFO << "Render pass destructed";
#endif  // !NDEBUG
}

}  // namespace lighter::renderer::vk
