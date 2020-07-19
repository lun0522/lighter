//
//  graphics_pass.cc
//
//  Created by Pujun Lun on 6/27/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/graphics_pass.h"

#include <string>

#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

using ImageUsageType = image::Usage::UsageType;

GraphicsPass::AttachmentIndexMap BuildAttachmentIndexMap(
    const BasePass::ImageUsageHistoryMap& image_usage_history_map) {
  GraphicsPass::AttachmentIndexMap attachment_index_map;
  int index = 0;
  for (const auto& pair : image_usage_history_map) {
    attachment_index_map.insert({pair.first, index++});
  }
  return attachment_index_map;
}

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

std::unique_ptr<GraphicsPass> GraphicsPassBuilder::Build() const {
  return std::unique_ptr<GraphicsPass>{new GraphicsPass(*this)};
}

GraphicsPass::GraphicsPass(const GraphicsPassBuilder& graphics_pass_builder)
    : attachment_index_map_{BuildAttachmentIndexMap(
          graphics_pass_builder.image_usage_history_map())} {
  RenderPassBuilder render_pass_builder{graphics_pass_builder.context_};
  SetAttachments(graphics_pass_builder, &render_pass_builder);
  render_pass_ = render_pass_builder.Build();
}

void GraphicsPass::SetAttachments(
    const GraphicsPassBuilder& graphics_pass_builder,
    RenderPassBuilder* render_pass_builder) {
  render_pass_builder->SetNumFramebuffers(
      graphics_pass_builder.image_usage_history_map().size());

  for (const auto& pair : graphics_pass_builder.image_usage_history_map()) {
    const Image& image = *pair.first;
    const image::UsageHistory& history = pair.second;

    RenderPassBuilder::Attachment attachment;
    attachment.initial_layout =
        graphics_pass_builder.GetImageLayoutBeforePass(image);
    attachment.final_layout =
        graphics_pass_builder.GetImageLayoutAfterPass(image);

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

    render_pass_builder->SetAttachment(attachment_index_map_.at(&image),
                                       attachment);
  }
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
