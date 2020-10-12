//
//  image_usage.cc
//
//  Created by Pujun Lun on 10/11/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/image_usage.h"

#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {

ImageUsageHistory& ImageUsageHistory::AddUsage(int subpass,
                                               const ImageUsage& usage) {
  const auto did_insert = usage_at_subpass_map_.insert({subpass, usage}).second;
  if (!did_insert) {
    FATAL(absl::StrFormat("Already specified usage for subpass %d", subpass));
  }
  return *this;
}

ImageUsageHistory& ImageUsageHistory::AddUsage(
    int subpass_start, int subpass_end, const ImageUsage& usage) {
  ASSERT_TRUE(subpass_start <= subpass_end,
              absl::StrFormat("Invalid range [%d, %d]",
                              subpass_start, subpass_end));
  for (int subpass = subpass_start; subpass <= subpass_end; ++subpass) {
    AddUsage(subpass, usage);
  }
  return *this;
}

ImageUsageHistory& ImageUsageHistory::SetFinalUsage(const ImageUsage& usage) {
  ASSERT_NO_VALUE(final_usage_, "Already specified final usage");
  final_usage_ = usage;
  return *this;
}

std::vector<ImageUsage> ImageUsageHistory::GetAllUsages() const {
  const int num_usages = 1 + static_cast<int>(usage_at_subpass_map_.size()) +
                         (final_usage_.has_value() ? 1 : 0);
  std::vector<ImageUsage> usages;
  usages.reserve(num_usages);
  usages.push_back(initial_usage_);
  for (const auto& pair : usage_at_subpass_map_) {
    usages.push_back(pair.second);
  }
  if (final_usage_.has_value()) {
    usages.push_back(final_usage_.value());
  }
  return usages;
}

ImageUsageTracker& ImageUsageTracker::TrackImage(
    std::string&& image_name, const ImageUsage& current_usage) {
  ASSERT_FALSE(image_usage_map_.contains(image_name),
               absl::StrFormat("Already tracking image with name '%s'",
                               image_name));
  image_usage_map_.insert({std::move(image_name), current_usage});
  return *this;
}

const ImageUsage& ImageUsageTracker::GetUsage(
    const std::string& image_name) const {
  const auto iter = image_usage_map_.find(image_name);
  ASSERT_FALSE(iter == image_usage_map_.end(),
               absl::StrFormat("Unrecognized image '%s'", image_name));
  return iter->second;
}

ImageUsageTracker& ImageUsageTracker::UpdateUsage(const std::string& image_name,
                                                  const ImageUsage& usage) {
  auto iter = image_usage_map_.find(image_name);
  ASSERT_FALSE(iter == image_usage_map_.end(),
               absl::StrFormat("Unrecognized image '%s'", image_name));
  iter->second = usage;
  return *this;
}

} /* namespace renderer */
} /* namespace lighter */
