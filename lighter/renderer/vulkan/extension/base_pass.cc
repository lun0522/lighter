//
//  base_pass.cc
//
//  Created by Pujun Lun on 7/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/base_pass.h"

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace image {

UsageHistory& UsageHistory::AddUsage(int subpass, const Usage& usage) {
  ASSERT_TRUE(
      usage_at_subpass_map_.find(subpass) == usage_at_subpass_map_.end(),
      absl::StrFormat("Already specified usage for image %s at subpass %d",
                      image_name_, subpass));
  usage_at_subpass_map_.insert({subpass, usage});
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
  ASSERT_NO_VALUE(final_usage_,
                  absl::StrFormat("Already specified final usage for image %s",
                                  image_name_));
  final_usage_ = usage;
  return *this;
}

const Usage& UsageHistory::GetUsage(int subpass) const {
  const auto iter = usage_at_subpass_map_.find(subpass);
  ASSERT_FALSE(iter == usage_at_subpass_map_.end(),
               absl::StrFormat("Usage not specified for image %s at subpass %d",
                               image_name_, subpass));
  return iter->second;
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

void UsageHistory::ValidateSubpasses(int upper_bound) const {
  for (const auto& pair : usage_at_subpass_map_) {
    const int subpass = pair.first;
    ASSERT_TRUE(
        subpass >= 0 && subpass < upper_bound,
        absl::StrFormat("Subpass (%d) out of range [0, %d) for image %s",
                        subpass, upper_bound, image_name_));
  }
}

} /* namespace image */

constexpr int BasePass::kVirtualInitialSubpassIndex;

BasePass& BasePass::AddImage(Image* image, image::UsageHistory&& history) {
  ASSERT_NON_NULL(image, "'image' cannot be nullptr");
  history.ValidateSubpasses(num_subpasses_);
  history.AddUsage(kVirtualInitialSubpassIndex, image->image_usage());
  image_usage_history_map_.emplace(image, std::move(history));
  return *this;
}

VkImageLayout BasePass::GetImageLayoutBeforePass(const Image& image) const {
  const auto& usage_at_subpass_map =
      GetUsageHistory(image).usage_at_subpass_map();
  return usage_at_subpass_map.at(kVirtualInitialSubpassIndex).GetImageLayout();
}

VkImageLayout BasePass::GetImageLayoutAfterPass(const Image& image) const {
  const image::UsageHistory& history = GetUsageHistory(image);
  const image::Usage& last_usage =
      history.final_usage().value_or(
          history.usage_at_subpass_map().rbegin()->second);
  return last_usage.GetImageLayout();
}

VkImageLayout BasePass::GetImageLayoutAtSubpass(const Image& image,
                                                int subpass) const {
  return GetUsageHistory(image).GetUsage(subpass).GetImageLayout();
}

const image::UsageHistory& BasePass::GetUsageHistory(const Image& image) const {
  const auto iter = image_usage_history_map_.find(&image);
  ASSERT_FALSE(iter == image_usage_history_map_.end(), "Unrecognized image");
  return iter->second;
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
