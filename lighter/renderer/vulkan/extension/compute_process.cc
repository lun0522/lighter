//
//  compute_process.cc
//
//  Created by Pujun Lun on 6/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/compute_process.h"

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {

ImageUsageHistory::ImageUsageHistory(
    const Image* image, std::string&& image_name, int num_stages)
    : image_{*FATAL_IF_NULL(image)}, image_name_{std::move(image_name)},
      num_stages_{num_stages} {
  usage_at_stage_map_.insert({-1, image_.image_usage()});
}

void ImageUsageHistory::AddUsageAtStage(int stage, const image::Usage& usage) {
  ValidateStage(stage);
  usage.Validate();
  if (IsImageUsedAtStage(stage)) {
    FATAL(
        absl::StrFormat("Duplicated usage specified for image %s at stage %d",
                        image_name_, stage));
  }
  usage_at_stage_map_.insert({stage, usage});
}

void ImageUsageHistory::ValidateStage(int stage) const {
  ASSERT_TRUE(
      stage >= 0 && stage < num_stages_,
      absl::StrFormat("Stage must be in range [0, %d), while %d provided",
                      num_stages_, stage));
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
