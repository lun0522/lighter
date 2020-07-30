//
//  graphics_pass.cc
//
//  Created by Pujun Lun on 6/27/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/graphics_pass.h"

#include <algorithm>
#include <map>
#include <vector>

#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/variant.h"

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

int GraphicsPass::AddAttachment(
    const std::string& image_name,
    GetLocation&& get_location, image::UsageHistory&& history,
    const absl::optional<AttachmentLoadStoreOps>& load_store_ops) {
  const absl::optional<int> subpass_requiring_location_getter =
      GetFirstSubpassRequiringLocationGetter(history);
  if (subpass_requiring_location_getter.has_value()) {
    ASSERT_NON_NULL(
        get_location,
        absl::StrFormat("Image '%s' is used as render target at subpass %d, "
                        "'get_location' must not be nullptr",
                        image_name, subpass_requiring_location_getter.value()));
    color_attachment_location_getter_map_.insert(
        {image_name, std::move(get_location)});
  }

  switch (GetImageUsageTypeForAllSubpasses(image_name, history)) {
    case ImageUsageType::kRenderTarget:
      if (load_store_ops.has_value()) {
        using ColorLoadStoreOps =
            RenderPassBuilder::Attachment::ColorLoadStoreOps;
        ASSERT_TRUE(
            absl::holds_alternative<ColorLoadStoreOps>(load_store_ops.value()),
            absl::StrFormat("Image '%s' is used as color attachment, but depth "
                            "stencil attachment load store ops are provided",
                            image_name));
        attachment_load_store_ops_map_.insert(
            {image_name, load_store_ops.value()});
      } else {
        attachment_load_store_ops_map_.insert(
            {image_name, GetDefaultColorLoadStoreOps()});
      }
      break;

    case ImageUsageType::kDepthStencil:
      if (load_store_ops.has_value()) {
        using DepthStencilLoadStoreOps =
            RenderPassBuilder::Attachment::DepthStencilLoadStoreOps;
        ASSERT_TRUE(
            absl::holds_alternative<DepthStencilLoadStoreOps>(
                load_store_ops.value()),
            absl::StrFormat("Image '%s' is used as depth stencil attachment, "
                            "but color attachment load store ops are provided",
                            image_name));
        attachment_load_store_ops_map_.insert(
            {image_name, load_store_ops.value()});
      } else {
        attachment_load_store_ops_map_.insert(
            {image_name, GetDefaultDepthStencilLoadStoreOps()});
      }
      break;

    default:
      FATAL("Unreachable");
  }

  BasePass::AddUsageHistory(std::string{image_name}, std::move(history));
  const auto attachment_index =
      static_cast<int>(image_usage_history_map().size()) - 1;
  attachment_index_map_.insert({image_name, attachment_index});
  return attachment_index;
}

GraphicsPass& GraphicsPass::AddMultisampleResolving(
    const std::string& src_image_name, const std::string& dst_image_name,
    int subpass) {
  // Check that source image history exists and its usage is expected.
  const auto src_iter = image_usage_history_map().find(src_image_name);
  ASSERT_FALSE(src_iter == image_usage_history_map().end(),
               "Usage history not specified for source image");
  const image::UsageHistory& src_history = src_iter->second;
  ASSERT_TRUE(
      CheckImageUsageType(src_history, subpass, ImageUsageType::kRenderTarget),
      absl::StrFormat("Usage type of source image '%s' at subpass %d must be"
                      "kRenderTarget",
                      src_image_name, subpass));

  // Check that destination image history exists and its usage is expected.
  const auto dst_iter = image_usage_history_map().find(dst_image_name);
  ASSERT_FALSE(dst_iter == image_usage_history_map().end(),
               "Usage history not specified for destination image");
  const image::UsageHistory& dst_history = dst_iter->second;
  ASSERT_TRUE(
      CheckImageUsageType(dst_history, subpass,
                          ImageUsageType::kMultisampleResolve),
      absl::StrFormat("Usage type of destination image '%s' at subpass %d must "
                      "be kMultisampleResolve",
                      dst_image_name, subpass));

  // Create multisampling pair.
  MultisamplingMap& multisampling_map = multisampling_at_subpass_maps_[subpass];
  const bool did_insert =
      multisampling_map.insert({src_image_name, dst_image_name}).second;
  ASSERT_TRUE(did_insert,
              absl::StrFormat("Already specified multisample resolving for "
                              "image '%s' at subpass %d",
                              src_image_name, subpass));
  return *this;
}

std::unique_ptr<RenderPassBuilder> GraphicsPass::CreateRenderPassBuilder(
    int num_framebuffers) {
  render_pass_builder_ = absl::make_unique<RenderPassBuilder>(context_);
  render_pass_builder_->SetNumFramebuffers(num_framebuffers);
  SetAttachments();
  SetSubpasses();
  SetSubpassDependencies();
  return std::move(render_pass_builder_);
}

void GraphicsPass::SetAttachments() {
  for (const auto& pair : image_usage_history_map()) {
    const std::string& image_name = pair.first;
    render_pass_builder_->SetAttachment(
        attachment_index_map_[image_name],
        RenderPassBuilder::Attachment{
            attachment_load_store_ops_map_[image_name],
            GetImageLayoutBeforePass(image_name),
            GetImageLayoutAfterPass(image_name),
        });
  }
}

void GraphicsPass::SetSubpasses() {
  using ColorAttachmentInfo = RenderPassBuilder::ColorAttachmentInfo;
  using MultisampleResolveInfo = RenderPassBuilder::MultisampleResolveInfo;

  for (int subpass = 0; subpass < num_subpasses_; ++subpass) {
    const MultisamplingMap& multisampling_map =
        multisampling_at_subpass_maps_[subpass];

    std::vector<MultisampleResolveInfo> multisample_resolve_infos;
    multisample_resolve_infos.reserve(multisampling_map.size());

    std::vector<ColorAttachmentInfo> color_attachment_infos;
    VkAttachmentReference depth_stencil_ref{
        VK_ATTACHMENT_UNUSED,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    for (const auto& pair : image_usage_history_map()) {
      const std::string& image_name = pair.first;
      const image::Usage* usage = GetImageUsage(image_name, subpass);
      if (usage == nullptr) {
        continue;
      }

      const VkAttachmentReference attachment_ref{
          static_cast<uint32_t>(attachment_index_map_[image_name]),
          usage->GetImageLayout(),
      };

      switch (usage->usage_type()) {
        case ImageUsageType::kRenderTarget: {
          const int location =
              color_attachment_location_getter_map_[image_name](subpass);
#ifndef NDEBUG
          LOG_INFO << absl::StreamFormat("Bind image '%s' to location %d at "
                                         "subpass %d",
                                         image_name, location, subpass);
#endif  /* !NDEBUG */

          const auto iter = multisampling_map.find(image_name);
          if (iter != multisampling_map.end()) {
            const std::string& target = iter->second;
            const int target_description_index = attachment_index_map_[target];
            const image::Usage* target_usage = GetImageUsage(target, subpass);
            ASSERT_NON_NULL(
                target_usage,
                absl::StrFormat("Target image '%s' is not used at subpass %d",
                                target, subpass));
            multisample_resolve_infos.push_back(MultisampleResolveInfo{
                location, target_description_index,
                target_usage->GetImageLayout(),
            });

#ifndef NDEBUG
            LOG_INFO << absl::StreamFormat("Image '%s' resolves to '%s' at "
                                           "subpass %d",
                                           image_name, target, subpass);
#endif  /* !NDEBUG */
          }

          color_attachment_infos.push_back(ColorAttachmentInfo{
              location,
              /*description_index=*/static_cast<int>(attachment_ref.attachment),
              attachment_ref.layout,
          });

          break;
        }

        case ImageUsageType::kDepthStencil:
          ASSERT_TRUE(depth_stencil_ref.attachment == VK_ATTACHMENT_UNUSED,
                      absl::StrFormat("Multiple depth stencil attachment "
                                      "specified for subpass %d",
                                      subpass));
          depth_stencil_ref = attachment_ref;
          break;

        case ImageUsageType::kMultisampleResolve:
          break;

        default:
          FATAL("Unreachable");
      }
    }

    auto color_refs = RenderPassBuilder::CreateColorAttachmentReferences(
        color_attachment_infos);
    auto multisampling_refs = RenderPassBuilder::CreateMultisamplingReferences(
        color_refs.size(), multisample_resolve_infos);
    render_pass_builder_->SetSubpass(subpass, std::move(color_refs),
                                     std::move(multisampling_refs),
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
          GetImageUsagesIfNeedSynchronization(pair.first, subpass);
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
    }

    for (const auto& pair : dependency_map) {
      const SubpassDependency& dependency = pair.second;
      render_pass_builder_->AddSubpassDependency(dependency);

#ifndef NDEBUG
      const std::string src_subpass_name =
          dependency.src_subpass.index == kExternalSubpassIndex
              ? "previous pass"
              : absl::StrFormat("subpass %d", dependency.src_subpass.index);
      const std::string dst_subpass_name =
          dependency.dst_subpass.index == kExternalSubpassIndex
              ? "next pass"
              : absl::StrFormat("subpass %d", dependency.dst_subpass.index);
      LOG_INFO << absl::StreamFormat("Added dependency from %s to %s",
                                     src_subpass_name, dst_subpass_name);
#endif  /* !NDEBUG */
    }
  }
}

absl::optional<int> GraphicsPass::GetFirstSubpassRequiringLocationGetter(
    const image::UsageHistory& history) const {
  for (const auto& pair : history.usage_at_subpass_map()) {
    const int subpass = pair.first;
    if (IsVirtualSubpass(subpass)) {
      continue;
    }
    if (pair.second.usage_type() == ImageUsageType::kRenderTarget) {
      return subpass;
    }
  }
  return absl::nullopt;
}

ImageUsageType GraphicsPass::GetImageUsageTypeForAllSubpasses(
    const std::string& image_name,
    const image::UsageHistory& history) const {
  ImageUsageType prev_usage_type = ImageUsageType::kDontCare;
  for (const auto& pair : history.usage_at_subpass_map()) {
    if (IsVirtualSubpass(pair.first)) {
      continue;
    }

    ImageUsageType usage_type = pair.second.usage_type();
    if (usage_type == ImageUsageType::kMultisampleResolve) {
      usage_type = ImageUsageType::kRenderTarget;
    }

    if (prev_usage_type == ImageUsageType::kDontCare) {
      prev_usage_type = usage_type;
    } else if (usage_type != prev_usage_type) {
      FATAL(absl::StrFormat(
          "Inconsistent usage type specified for image '%s' (previous %d, "
          "current %d)",
          image_name, prev_usage_type, usage_type));
    }
  }

  if (prev_usage_type == ImageUsageType::kDontCare) {
    FATAL(absl::StrFormat("Image '%s' has no usage specified (excluding "
                          "initial and final usage)",
                          image_name));
  }

  return prev_usage_type;
}

bool GraphicsPass::CheckImageUsageType(
    const image::UsageHistory& history, int subpass,
    ImageUsageType usage_type) const {
  const auto iter = history.usage_at_subpass_map().find(subpass);
  return iter != history.usage_at_subpass_map().end() &&
         iter->second.usage_type() == usage_type;
}

void GraphicsPass::ValidateImageUsageHistory(
    const std::string& image_name,
    const image::UsageHistory& history) const {
  for (const auto& pair : history.usage_at_subpass_map()) {
    const ImageUsageType usage_type = pair.second.usage_type();
    ASSERT_TRUE(usage_type == ImageUsageType::kRenderTarget
                    || usage_type == ImageUsageType::kDepthStencil
                    || usage_type == ImageUsageType::kMultisampleResolve,
                absl::StrFormat("Usage type of image '%s' at subpass %d must "
                                "be one of kRenderTarget, kDepthStencil or "
                                "kMultisampleResolve, while %d provided",
                                image_name, pair.first, usage_type));
  }
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
