//
//  base_pass.cc
//
//  Created by Pujun Lun on 7/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/base_pass.h"

#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace image {

UsageHistory& UsageHistory::AddUsage(int subpass, const Usage& usage) {
  const auto did_insert = usage_at_subpass_map_.insert({subpass, usage}).second;
  if (!did_insert) {
    FATAL(absl::StrFormat(
        "Already specified usage for image '%s' at subpass %d",
        image_name_, subpass));
  }
  return *this;
}

UsageHistory& UsageHistory::AddUsage(int subpass_start, int subpass_end,
                                     const Usage& usage) {
  for (int subpass = subpass_start; subpass <= subpass_end; ++subpass) {
    AddUsage(subpass, usage);
  }
  return *this;
}

UsageHistory& UsageHistory::SetFinalUsage(const Usage& usage) {
  ASSERT_NO_VALUE(
      final_usage_,
      absl::StrFormat("Already specified final usage for image '%s'",
                      image_name_));
  final_usage_ = usage;
  return *this;
}

std::vector<Usage> UsageHistory::GetAllUsages() const {
  const int num_usages = static_cast<int>(usage_at_subpass_map_.size()) +
                         (final_usage_.has_value() ? 1 : 0);
  std::vector<Usage> usages;
  usages.reserve(num_usages);
  for (const auto& pair : usage_at_subpass_map_) {
    usages.push_back(pair.second);
  }
  if (final_usage_.has_value()) {
    usages.push_back(final_usage_.value());
  }
  return usages;
}

} /* namespace image */

BasePass& BasePass::AddImage(Image* image, image::UsageHistory&& history) {
  ASSERT_NON_NULL(image, "'image' cannot be nullptr");
  for (const auto& pair : history.usage_at_subpass_map()) {
    ValidateSubpass(/*subpass=*/pair.first, history.image_name(),
                    /*include_virtual_subpasses=*/false);
  }
  ValidateImageUsageHistory(history);

  history.AddUsage(virtual_initial_subpass_index(), image->image_usage());
  if (history.final_usage().has_value()) {
    history.AddUsage(virtual_final_subpass_index(),
                     history.final_usage().value());
  }
  image_usage_history_map_.emplace(image, std::move(history));
  return *this;
}

VkImageLayout BasePass::GetImageLayoutBeforePass(const Image& image) const {
  const auto& first_usage =
      GetUsageHistory(image).usage_at_subpass_map().begin()->second;
  return first_usage.GetImageLayout();
}

VkImageLayout BasePass::GetImageLayoutAfterPass(const Image& image) const {
  const auto& last_usage =
      GetUsageHistory(image).usage_at_subpass_map().rbegin()->second;
  return last_usage.GetImageLayout();
}

VkImageLayout BasePass::GetImageLayoutAtSubpass(const Image& image,
                                                int subpass) const {
  const image::UsageHistory& history = GetUsageHistory(image);
  ValidateSubpass(subpass, history.image_name(),
                  /*include_virtual_subpasses=*/false);
  const image::Usage* usage = GetImageUsage(history, subpass);
  ASSERT_NON_NULL(
      usage,
      absl::StrFormat("Usage not specified for image '%s' at subpass %d",
                      history.image_name(), subpass));
  return usage->GetImageLayout();
}

const image::Usage* BasePass::GetImageUsage(const image::UsageHistory& history,
                                            int subpass) const {
  ValidateSubpass(subpass, history.image_name(),
                  /*include_virtual_subpasses=*/true);
  const auto iter = history.usage_at_subpass_map().find(subpass);
  return iter == history.usage_at_subpass_map().end() ? nullptr : &iter->second;
}

absl::optional<BasePass::ImageUsagesInfo>
BasePass::GetImageUsagesIfNeedSynchronization(
    const image::UsageHistory& history, int subpass) const {
  ValidateSubpass(subpass, history.image_name(),
                  /*include_virtual_subpasses=*/true);
  const auto curr_usage_iter = history.usage_at_subpass_map().find(subpass);
  if (curr_usage_iter == history.usage_at_subpass_map().end()) {
    return absl::nullopt;
  }
  const auto prev_usage_iter = std::prev(curr_usage_iter);

  const image::Usage& prev_usage = prev_usage_iter->second;
  const image::Usage& curr_usage = curr_usage_iter->second;
  if (!image::NeedSynchronization(prev_usage, curr_usage)) {
    return absl::nullopt;
  }

  const int prev_usage_subpass = prev_usage_iter->first;
  return ImageUsagesInfo{prev_usage_subpass, &prev_usage, &curr_usage};
}

const image::UsageHistory& BasePass::GetUsageHistory(const Image& image) const {
  const auto iter = image_usage_history_map_.find(&image);
  ASSERT_FALSE(iter == image_usage_history_map_.end(), "Unrecognized image");
  return iter->second;
}

void BasePass::ValidateSubpass(int subpass,
                               const std::string& image_name,
                               bool include_virtual_subpasses) const {
  if (include_virtual_subpasses) {
    ASSERT_TRUE(
        subpass >= virtual_initial_subpass_index()
        && subpass <= virtual_final_subpass_index(),
        absl::StrFormat("Subpass (%d) out of range [%d, %d] for image '%s'",
                        subpass, virtual_initial_subpass_index(),
                        virtual_final_subpass_index(), image_name));
  } else {
    ASSERT_TRUE(
        subpass >= 0 && subpass < num_subpasses_,
        absl::StrFormat("Subpass (%d) out of range [0, %d) for image '%s'",
                        subpass, num_subpasses_, image_name));
  }
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
