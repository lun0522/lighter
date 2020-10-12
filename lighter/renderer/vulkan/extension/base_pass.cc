//
//  base_pass.cc
//
//  Created by Pujun Lun on 7/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/base_pass.h"

#include "lighter/renderer/vulkan/wrapper/image_util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {

void BasePass::AddUsageHistory(std::string&& image_name,
                               ImageUsageHistory&& history) {
  for (const auto& pair : history.usage_at_subpass_map()) {
    ValidateSubpass(/*subpass=*/pair.first, image_name,
                    /*include_virtual_subpasses=*/false);
  }

  history.AddUsage(virtual_initial_subpass_index(), history.initial_usage());
  if (history.final_usage().has_value()) {
    history.AddUsage(virtual_final_subpass_index(),
                     history.final_usage().value());
  }
  image_usage_history_map_.emplace(std::move(image_name), std::move(history));
}

VkImageLayout BasePass::GetImageLayoutBeforePass(
    const std::string& image_name) const {
  const auto& first_usage =
      GetUsageHistory(image_name).usage_at_subpass_map().begin()->second;
  return image::GetImageLayout(first_usage);
}

VkImageLayout BasePass::GetImageLayoutAfterPass(
    const std::string& image_name) const {
  const auto& last_usage =
      GetUsageHistory(image_name).usage_at_subpass_map().rbegin()->second;
  return image::GetImageLayout(last_usage);
}

VkImageLayout BasePass::GetImageLayoutAtSubpass(const std::string& image_name,
                                                int subpass) const {
  ValidateSubpass(subpass, image_name, /*include_virtual_subpasses=*/false);
  const ImageUsage* usage = GetImageUsage(image_name, subpass);
  ASSERT_NON_NULL(
      usage,
      absl::StrFormat("Usage not specified for image '%s' at subpass %d",
                      image_name, subpass));
  return image::GetImageLayout(*usage);
}

void BasePass::UpdateTrackedImageUsage(
    const std::string& image_name,
    ImageUsageTracker& usage_tracker) const {
  const auto& last_usage =
      GetUsageHistory(image_name).usage_at_subpass_map().rbegin()->second;
  usage_tracker.UpdateUsage(image_name, last_usage);

#ifndef NDEBUG
  LOG_INFO << absl::StreamFormat("Updated tracked usage for image '%s'",
                                 image_name);
#endif /* !NDEBUG */
}

const ImageUsageHistory& BasePass::GetUsageHistory(
    const std::string& image_name) const {
  const auto iter = image_usage_history_map_.find(image_name);
  ASSERT_FALSE(iter == image_usage_history_map_.end(),
               absl::StrFormat("Unrecognized image '%s'", image_name));
  return iter->second;
}

const ImageUsage* BasePass::GetImageUsage(const std::string& image_name,
                                          int subpass) const {
  ValidateSubpass(subpass, image_name, /*include_virtual_subpasses=*/true);
  const ImageUsageHistory& history = GetUsageHistory(image_name);
  const auto iter = history.usage_at_subpass_map().find(subpass);
  return iter == history.usage_at_subpass_map().end() ? nullptr : &iter->second;
}

absl::optional<BasePass::ImageUsagesInfo>
BasePass::GetImageUsagesIfNeedSynchronization(
    const std::string& image_name, int subpass) const {
  ValidateSubpass(subpass, image_name, /*include_virtual_subpasses=*/true);
  const ImageUsageHistory& history = GetUsageHistory(image_name);
  const auto curr_usage_iter = history.usage_at_subpass_map().find(subpass);
  if (curr_usage_iter == history.usage_at_subpass_map().end()) {
    return absl::nullopt;
  }
  const auto prev_usage_iter = std::prev(curr_usage_iter);

  const ImageUsage& prev_usage = prev_usage_iter->second;
  const ImageUsage& curr_usage = curr_usage_iter->second;
  if (!image::NeedSynchronization(prev_usage, curr_usage)) {
    return absl::nullopt;
  }

  const int prev_usage_subpass = prev_usage_iter->first;
  return ImageUsagesInfo{prev_usage_subpass, &prev_usage, &curr_usage};
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
