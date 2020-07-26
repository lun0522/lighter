//
//  image_util.cc
//
//  Created by Pujun Lun on 7/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/image_util.h"

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace image {

UsageTracker& UsageTracker::TrackImage(std::string&& image_name,
                                       const Usage& initial_usage) {
  ASSERT_FALSE(image_usage_map_.contains(image_name),
               absl::StrFormat("Already tracking image with name '%s'",
                               image_name));
  image_usage_map_.insert({std::move(image_name), initial_usage});
  return *this;
}

const Usage& UsageTracker::GetUsage(const std::string& image_name) const {
  const auto iter = image_usage_map_.find(image_name);
  ASSERT_FALSE(iter == image_usage_map_.end(),
               absl::StrFormat("Unrecognized image '%s'", image_name));
  return iter->second;
}

UsageTracker& UsageTracker::UpdateUsage(const std::string& image_name,
                                        const Usage& usage) {
  auto iter = image_usage_map_.find(image_name);
  ASSERT_FALSE(iter == image_usage_map_.end(),
               absl::StrFormat("Unrecognized image '%s'", image_name));
  iter->second = usage;
  return *this;
}

UsageHistory& UsageHistory::AddUsage(int subpass, const Usage& usage) {
  const auto did_insert = usage_at_subpass_map_.insert({subpass, usage}).second;
  if (!did_insert) {
    FATAL(absl::StrFormat("Already specified usage for subpass %d", subpass));
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
  ASSERT_NO_VALUE(final_usage_, "Already specified final usage");
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
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
