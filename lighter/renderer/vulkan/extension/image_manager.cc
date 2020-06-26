//
//  image_manager.cc
//
//  Created by Pujun Lun on 6/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/image_manager.h"

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {

ImageUsageManager::ImageUsageManager(
    const Image* image, int num_stages,
    absl::Span<const image::UsageAtStage> usage_at_stages)
    : image_{*FATAL_IF_NULL(image)}, num_stages_{num_stages} {
  usage_at_stage_map_.insert({-1, image_.initial_usage()});
  for (const auto& usage_at_stage : usage_at_stages) {
    ValidateStage(usage_at_stage.stage);
    usage_at_stage.usage.Validate();
    usage_at_stage_map_.insert({usage_at_stage.stage, usage_at_stage.usage});
  }
}

void ImageUsageManager::ValidateStage(int stage) const {
  ASSERT_TRUE(
      stage >= 0 && stage < num_stages_,
      absl::StrFormat("Stage must be in range [0, %d], while %d provided",
                      num_stages_ - 1, stage));
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
