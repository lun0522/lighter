//
//  graphics_pass.cc
//
//  Created by Pujun Lun on 6/27/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/graphics_pass.h"

#include <string>
#include <vector>

#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

using ImageUsageType = image::Usage::UsageType;

ImageUsageType GetImageUsageTypeForAllSubpasses(
    const image::UsageHistory& history) {
  ImageUsageType prev_usage_type = ImageUsageType::kDontCare;
  for (const auto& pair : history.usage_at_subpass_map()) {
    const ImageUsageType usage_type = pair.second.usage_type();
    switch (usage_type) {
      case ImageUsageType::kDontCare:
        continue;

      case ImageUsageType::kRenderTarget:
      case ImageUsageType::kDepthStencil:
        if (prev_usage_type == ImageUsageType::kDontCare) {
          prev_usage_type = usage_type;
        } else if (prev_usage_type != usage_type) {
          FATAL(absl::StrFormat(
              "Inconsistent usage type specified for image %s (previous %d, "
              "current %d)",
              history.image_name(), prev_usage_type, usage_type));
        }
        break;

      default:
        FATAL(absl::StrFormat("Usage type (%d) is neither kRenderTarget nor "
                              "kDepthStencil for image %s",
                              pair.second.usage_type(), history.image_name()));
    }
  }
  return prev_usage_type;
}

} /* namespace */

std::unique_ptr<GraphicsPass> GraphicsPassBuilder::Build() {
  render_pass_builder_ = absl::make_unique<RenderPassBuilder>(context_);
  RebuildAttachmentIndexMap();
  SetAttachments();
  SetSubpasses();
  SetSubpassDependencies();
  std::unique_ptr<RenderPass> render_pass = render_pass_builder_->Build();
  render_pass_builder_.reset();

  return std::unique_ptr<GraphicsPass>{
      new GraphicsPass(std::move(render_pass),
                       std::move(attachment_index_map_))};
}

void GraphicsPassBuilder::RebuildAttachmentIndexMap() {
  attachment_index_map_.clear();
  int index = 0;
  for (const auto& pair : image_usage_history_map()) {
    attachment_index_map_.insert({pair.first, index++});
  }
}

void GraphicsPassBuilder::SetAttachments() {
  render_pass_builder_->SetNumFramebuffers(image_usage_history_map().size());

  for (const auto& pair : image_usage_history_map()) {
    const Image& image = *pair.first;
    const image::UsageHistory& history = pair.second;

    RenderPassBuilder::Attachment attachment;
    attachment.initial_layout = GetImageLayoutBeforePass(image);
    attachment.final_layout = GetImageLayoutAfterPass(image);

    switch (GetImageUsageTypeForAllSubpasses(history)) {
      case ImageUsageType::kDontCare:
        FATAL(absl::StrFormat("Image %s is not used in subpasses",
                              history.image_name()));

      case ImageUsageType::kRenderTarget:
        attachment.attachment_ops = RenderPassBuilder::Attachment::ColorOps{
            /*load_color_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
            /*store_color_op=*/VK_ATTACHMENT_STORE_OP_STORE,
        };
        break;

      case ImageUsageType::kDepthStencil:
        attachment.attachment_ops =
            RenderPassBuilder::Attachment::DepthStencilOps{
                /*load_depth_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                /*store_depth_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
                /*load_stencil_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                /*store_stencil_op=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,
            };
        break;

      default:
        FATAL("Unreachable");
    }

    render_pass_builder_->SetAttachment(attachment_index_map_[&image],
                                        attachment);
  }
}

void GraphicsPassBuilder::SetSubpasses() {
  for (int subpass = 0; subpass < num_subpasses_; ++subpass) {
    std::vector<VkAttachmentReference> color_refs;
    VkAttachmentReference depth_stencil_ref{
        VK_ATTACHMENT_UNUSED,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    for (const auto& pair : image_usage_history_map()) {
      const image::Usage* usage = pair.second.GetUsage(subpass);
      if (usage == nullptr) {
        continue;
      }

      const VkAttachmentReference attachment_ref{
          static_cast<uint32_t>(attachment_index_map_[pair.first]),
          usage->GetImageLayout(),
      };
      switch (usage->usage_type()) {
        case ImageUsageType::kRenderTarget:
          color_refs.push_back(attachment_ref);
          break;

        case ImageUsageType::kDepthStencil:
          ASSERT_TRUE(depth_stencil_ref.attachment == VK_ATTACHMENT_UNUSED,
                      absl::StrFormat("Multiple depth stencil attachment "
                                      "specified for subpass %d",
                                      subpass));
          depth_stencil_ref = attachment_ref;
          break;

        default:
          FATAL("Unreachable");
      }
    }

    render_pass_builder_->SetSubpass(subpass, std::move(color_refs),
                                     depth_stencil_ref);
  }
}

void GraphicsPassBuilder::SetSubpassDependencies() {

}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
