//
//  render_pass.cc
//
//  Created by Pujun Lun on 1/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/render_pass.h"

#include "lighter/common/util.h"
#include "lighter/renderer/pipeline.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk {
namespace {

std::vector<VkAttachmentDescription> CreateAttachmentDescriptions(
    absl::Span<const DeviceImage* const> attachments) {
  FATAL("Not yet implemented");
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
    attachments.push_back(pair.first);
  }
  if (pipeline_descriptor.depth_stencil_attachment != nullptr) {
    attachments.push_back(pipeline_descriptor.depth_stencil_attachment);
  }

  const auto attachment_descriptions =
      CreateAttachmentDescriptions(attachments);
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

}  // namespace vk::renderer::lighter
