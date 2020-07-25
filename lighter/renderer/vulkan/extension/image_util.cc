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
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
