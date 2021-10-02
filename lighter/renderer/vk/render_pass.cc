//
//  render_pass.cc
//
//  Created by Pujun Lun on 1/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/render_pass.h"

#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter::renderer::vk {
namespace {

using ir::RenderPassDescriptor;
using ir::SubpassDescriptor;

class RenderPassBuilder {
 public:
  static intl::RenderPass Build(const Context& context,
                                const RenderPassDescriptor& descriptor) {
    const RenderPassBuilder builder{context, descriptor};
    return builder.render_pass_;
  }

  // This class is neither copyable nor movable.
  RenderPassBuilder(const RenderPassBuilder&) = delete;
  RenderPassBuilder& operator=(const RenderPassBuilder&) = delete;

 private:
  RenderPassBuilder(const Context& context,
                    const RenderPassDescriptor& descriptor);
  void CreateAttachments();
  void CreateSubpassDescriptions();
  void CreateSubpassDependencies();

  const RenderPassDescriptor& descriptor_;
  std::vector<intl::AttachmentDescription> attachment_descriptions_;
  absl::flat_hash_map<const ir::Image*, int> attachment_index_map_;
  std::vector<intl::AttachmentReference> attachment_references_;
  std::vector<intl::SubpassDescription> subpass_descriptions_;
  std::vector<intl::SubpassDependency> subpass_dependencies_;
  intl::RenderPass render_pass_;
};

RenderPassBuilder::RenderPassBuilder(const Context& context,
                                     const RenderPassDescriptor& descriptor)
    : descriptor_{descriptor} {
  CreateAttachments();
  CreateSubpassDescriptions();
  CreateSubpassDependencies();

  const auto render_pass_create_info = intl::RenderPassCreateInfo{}
      .setAttachments(attachment_descriptions_)
      .setSubpasses(subpass_descriptions_)
      .setDependencies(subpass_dependencies_);
  render_pass_ = context.device()->createRenderPass(
      render_pass_create_info, *context.host_allocator());
}

// TODO: Populate image layout.
void RenderPassBuilder::CreateAttachments() {
  for (const auto& [attachment, load_store_ops] : descriptor_.color_ops_map) {
    attachment_index_map_.insert(
        {attachment, static_cast<int>(attachment_descriptions_.size())});
    const auto& vk_attachment = Image::Cast(*attachment);
    attachment_descriptions_.push_back(intl::AttachmentDescription{}
        .setFormat(vk_attachment.format())
        .setSamples(vk_attachment.sample_count())
        .setLoadOp(type::ConvertAttachmentLoadOp(load_store_ops.load_op))
        .setStoreOp(type::ConvertAttachmentStoreOp(load_store_ops.store_op))
        .setStencilLoadOp(intl::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(intl::AttachmentStoreOp::eDontCare)
        .setInitialLayout(intl::ImageLayout::eGeneral)
        .setFinalLayout(intl::ImageLayout::eGeneral));
  }

  for (const auto& [attachment, load_store_ops] :
       descriptor_.depth_stencil_ops_map) {
    attachment_index_map_.insert(
        {attachment, static_cast<int>(attachment_descriptions_.size())});
    const auto& vk_attachment = Image::Cast(*attachment);
    attachment_descriptions_.push_back(intl::AttachmentDescription{}
        .setFormat(vk_attachment.format())
        .setSamples(vk_attachment.sample_count())
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
  }
}

// TODO: Populate image layout.
void RenderPassBuilder::CreateSubpassDescriptions() {
  int num_references = 0;
  for (const SubpassDescriptor& subpass : descriptor_.subpass_descriptors) {
    num_references += subpass.color_attachments.size();
    if (subpass.depth_stencil_attachment != nullptr) {
      ++num_references;
    }
  }
  attachment_references_.resize(
      num_references, intl::AttachmentReference{VK_ATTACHMENT_UNUSED});

  int reference_index = 0;
  const auto add_reference =
      [this, &reference_index](const ir::Image* attachment) {
        const auto iter = attachment_index_map_.find(attachment);
        ASSERT_FALSE(iter == attachment_index_map_.end(),
                     absl::StrFormat("Attachment '%s' not declared",
                                     attachment->name()));
        attachment_references_[reference_index++] = intl::AttachmentReference{}
            .setAttachment(iter->second)
            .setLayout(intl::ImageLayout::eGeneral);
      };

  subpass_descriptions_.reserve(descriptor_.subpass_descriptors.size());
  for (const SubpassDescriptor& subpass : descriptor_.subpass_descriptors) {
    const intl::AttachmentReference* color_refs_ptr =
        &attachment_references_[reference_index];
    for (const ir::Image* attachment : subpass.color_attachments) {
      add_reference(attachment);
    }

    const intl::AttachmentReference* depth_stencil_ref_ptr = nullptr;
    if (subpass.depth_stencil_attachment != nullptr) {
      depth_stencil_ref_ptr = &attachment_references_[reference_index];
      add_reference(subpass.depth_stencil_attachment);
    }

    subpass_descriptions_.push_back(intl::SubpassDescription{}
        .setPipelineBindPoint(intl::PipelineBindPoint::eGraphics)
        .setColorAttachmentCount(CONTAINER_SIZE(subpass.color_attachments))
        .setPColorAttachments(color_refs_ptr)
        .setPDepthStencilAttachment(depth_stencil_ref_ptr));
  }
}

// TODO: Set fields properly.
void RenderPassBuilder::CreateSubpassDependencies() {
  for (const auto& [from, to, attachments] : descriptor_.subpass_dependencies) {
    subpass_dependencies_.push_back(
      intl::SubpassDependency{}
          .setSrcSubpass(CAST_TO_UINT(from))
          .setDstSubpass(CAST_TO_UINT(to))
          .setSrcStageMask(intl::PipelineStageFlagBits::eAllGraphics)
          .setDstStageMask(intl::PipelineStageFlagBits::eAllGraphics)
          .setSrcAccessMask(intl::AccessFlagBits::eColorAttachmentWrite)
          .setDstAccessMask(intl::AccessFlagBits::eColorAttachmentWrite)
    );
  }
}

}  // namespace

RenderPass::RenderPass(const SharedContext& context,
                       const RenderPassDescriptor& descriptor)
    : WithSharedContext{context},
      render_pass_{RenderPassBuilder::Build(*context_, descriptor)},
      framebuffers_{context_, render_pass_, descriptor} {}

RenderPass::~RenderPass() {
  context_->DeviceDestroy(render_pass_);
#ifndef NDEBUG
  LOG_INFO << "Render pass destructed";
#endif  // DEBUG
}

}  // namespace lighter::renderer::vk
