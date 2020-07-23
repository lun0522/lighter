//
//  graphics_pass.cc
//
//  Created by Pujun Lun on 6/27/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/graphics_pass.h"

#include <map>
#include <string>
#include <vector>

#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

using ImageUsageType = image::Usage::UsageType;

// Specifies the image usage at the subpass described by 'subpass_info', so that
// it will be considered when setting up the subpass dependency.
void IncludeUsageInSubpassDependency(
    const image::Usage& image_usage,
    RenderPassBuilder::SubpassDependency::SubpassInfo* subpass_info) {
  subpass_info->stage_flags |= image_usage.GetPipelineStageFlags();
  subpass_info->access_flags |= image_usage.GetAccessFlags();
}

} /* namespace */

GraphicsPass::GraphicsPass(SharedBasicContext context, int num_subpasses)
    : BasePass{num_subpasses}, context_{std::move(FATAL_IF_NULL(context))} {
  multisampling_at_subpass_maps_.resize(num_subpasses_);
}

int GraphicsPass::AddImage(Image* image, image::UsageHistory&& history) {
  BasePass::AddImageAndHistory(image, std::move(history));
  const auto attachment_index =
      static_cast<int>(image_usage_history_map().size()) - 1;
  attachment_index_map_.insert({image, attachment_index});
  return attachment_index;
}

GraphicsPass& GraphicsPass::AddMultisampleResolving(
    const Image& src_image, const Image& dst_image, int subpass) {
  const auto src_iter = image_usage_history_map().find(&src_image);
  ASSERT_FALSE(src_iter == image_usage_history_map().end(),
               "Usage history not specified for source image");
  ASSERT_FALSE(src_image.sample_count() == VK_SAMPLE_COUNT_1_BIT,
               absl::StrFormat("Source image '%s' is not multisample image",
                               src_iter->second.image_name()));

  const auto dst_iter = image_usage_history_map().find(&dst_image);
  ASSERT_FALSE(dst_iter == image_usage_history_map().end(),
               "Usage history not specified for destination image");
  ASSERT_TRUE(
      dst_image.sample_count() == VK_SAMPLE_COUNT_1_BIT,
      absl::StrFormat("Destination image '%s' must not be multisample image",
                      dst_iter->second.image_name()));

  MultisamplingMap& multisampling_map = multisampling_at_subpass_maps_[subpass];
  const bool did_insert =
      multisampling_map.insert({&src_image, &dst_image}).second;
  ASSERT_TRUE(did_insert,
              absl::StrFormat("Already specified multisample resolving for "
                              "image '%s' at subpass %d",
                              src_iter->second.image_name(), subpass));
  return *this;
}

std::unique_ptr<RenderPassBuilder> GraphicsPass::CreateRenderPassBuilder() {
  render_pass_builder_ = absl::make_unique<RenderPassBuilder>(context_);
  SetAttachments();
  SetSubpasses();
  SetSubpassDependencies();
  return std::move(render_pass_builder_);
}

void GraphicsPass::SetAttachments() {
  render_pass_builder_->SetNumFramebuffers(image_usage_history_map().size());

  for (const auto& pair : image_usage_history_map()) {
    const Image& image = *pair.first;

    RenderPassBuilder::Attachment attachment;
    attachment.initial_layout = GetImageLayoutBeforePass(image);
    attachment.final_layout = GetImageLayoutAfterPass(image);

    switch (GetImageUsageTypeForAllSubpasses(pair.second)) {
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

void GraphicsPass::SetSubpasses() {
  using MultisamplingPair = RenderPassBuilder::MultisamplingPair;

  for (int subpass = 0; subpass < num_subpasses_; ++subpass) {
    const MultisamplingMap& multisampling_map =
        multisampling_at_subpass_maps_[subpass];

    std::vector<MultisamplingPair> multisampling_pairs;
    multisampling_pairs.reserve(multisampling_map.size());

    std::vector<VkAttachmentReference> color_refs;
    VkAttachmentReference depth_stencil_ref{
        VK_ATTACHMENT_UNUSED,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    for (const auto& pair : image_usage_history_map()) {
      const image::Usage* usage = GetImageUsage(pair.second, subpass);
      if (usage == nullptr) {
        continue;
      }
      const Image& image = *pair.first;

      const VkAttachmentReference attachment_ref{
          static_cast<uint32_t>(attachment_index_map_[&image]),
          usage->GetImageLayout(),
      };

      switch (usage->usage_type()) {
        case ImageUsageType::kRenderTarget: {
          const auto iter = multisampling_map.find(&image);
          if (iter != multisampling_map.end()) {
            const Image& target = *iter->second;
            multisampling_pairs.push_back(MultisamplingPair{
                /*multisample_reference=*/static_cast<int>(color_refs.size()),
                /*target_attachment=*/
                static_cast<uint32_t>(attachment_index_map_[&target]),
            });

#ifndef NDEBUG
            LOG_INFO << absl::StreamFormat("Image '%s' resolves to '%s' at "
                                           "subpass %d",
                                           GetUsageHistory(image).image_name(),
                                           GetUsageHistory(target).image_name(),
                                           subpass);
#endif  /* !NDEBUG */
          }
          color_refs.push_back(attachment_ref);
          break;
        }

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

    render_pass_builder_->SetMultisampling(
        subpass,
        RenderPassBuilder::CreateMultisamplingReferences(
            color_refs.size(), multisampling_pairs));
    render_pass_builder_->SetSubpass(subpass, std::move(color_refs),
                                     depth_stencil_ref);
  }
}

void GraphicsPass::SetSubpassDependencies() {
  using SubpassDependency = RenderPassBuilder::SubpassDependency;

  ASSERT_TRUE(virtual_final_subpass_index() == num_subpasses_,
              "Assumption of the following loop is broken");
  for (int subpass = 0; subpass <= num_subpasses_; ++subpass) {
    // Maps the source subpass index to the dependency between source subpass
    // and current subpass. We use an ordered map to make debugging easier.
    std::map<int, SubpassDependency> dependency_map;

    for (const auto& pair : image_usage_history_map()) {
      const auto usages_info =
          GetImageUsagesIfNeedSynchronization(/*history=*/pair.second, subpass);
      if (!usages_info.has_value()) {
        continue;
      }
      const image::Usage& prev_usage = usages_info.value().prev_usage;
      const image::Usage& curr_usage = usages_info.value().curr_usage;
      const int src_subpass = usages_info.value().prev_usage_subpass;

      auto iter = dependency_map.find(src_subpass);
      if (iter == dependency_map.end()) {
        const SubpassDependency default_dependency{
            /*src_subpass=*/SubpassDependency::SubpassInfo{
                RegulateSubpassIndex(src_subpass),
                /*stage_flags=*/nullflag,
                /*access_flags=*/nullflag,
            },
            /*dst_subpass=*/SubpassDependency::SubpassInfo{
                RegulateSubpassIndex(subpass),
                /*stage_flags=*/nullflag,
                /*access_flags=*/nullflag,
            },
            /*dependency_flags=*/nullflag,
        };
        iter = dependency_map.insert({src_subpass, default_dependency}).first;
      }

      IncludeUsageInSubpassDependency(prev_usage, &iter->second.src_subpass);
      IncludeUsageInSubpassDependency(curr_usage, &iter->second.dst_subpass);

#ifndef NDEBUG
      const std::string src_subpass_name =
          IsVirtualSubpass(src_subpass)
              ? "previous pass"
              : absl::StrFormat("subpass %d", src_subpass);
      const std::string dst_subpass_name =
          IsVirtualSubpass(subpass)
              ? "next pass"
              : absl::StrFormat("subpass %d", subpass);
      LOG_INFO << absl::StreamFormat("Added dependency from %s to %s",
                                     src_subpass_name, dst_subpass_name);
#endif  /* !NDEBUG */
    }

    for (const auto& pair : dependency_map) {
      render_pass_builder_->AddSubpassDependency(pair.second);
    }
  }
}

ImageUsageType GraphicsPass::GetImageUsageTypeForAllSubpasses(
    const image::UsageHistory& history) const {
  ImageUsageType prev_usage_type = ImageUsageType::kDontCare;
  for (const auto& pair : history.usage_at_subpass_map()) {
    if (IsVirtualSubpass(pair.first)) {
      continue;
    }

    const ImageUsageType usage_type = pair.second.usage_type();
    if (prev_usage_type == ImageUsageType::kDontCare) {
      prev_usage_type = usage_type;
    } else if (usage_type != prev_usage_type) {
      FATAL(absl::StrFormat(
          "Inconsistent usage type specified for image '%s' (previous %d, "
          "current %d)",
          history.image_name(), prev_usage_type, usage_type));
    }
  }
  return prev_usage_type;
}

void GraphicsPass::ValidateImageUsageHistory(
    const image::UsageHistory& history) const {
  for (const auto& pair : history.usage_at_subpass_map()) {
    const ImageUsageType usage_type = pair.second.usage_type();
    ASSERT_TRUE(usage_type == ImageUsageType::kRenderTarget
                    || usage_type == ImageUsageType::kDepthStencil,
                absl::StrFormat("Usage type (%d) is neither kRenderTarget nor "
                                "kDepthStencil for image '%s' at subpass %d",
                                usage_type, history.image_name(), pair.first));
  }
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
