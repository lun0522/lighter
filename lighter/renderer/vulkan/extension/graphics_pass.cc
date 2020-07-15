//
//  graphics_pass.cc
//
//  Created by Pujun Lun on 6/27/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/graphics_pass.h"

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {

GraphicsPass& GraphicsPass::AddImage(Image* image,
                                     ImageGraphicsUsageHistory&& history) {
  ASSERT_NON_NULL(image, "'image' cannot be nullptr");
  ASSERT_TRUE(history.usage_history_.size() - 1 == num_subpasses_,
              absl::StrFormat("Number of subpasses in usage history (%d) "
                              "mismatches with graphics pass (%d)",
                              history.usage_history_.size() - 1,
                              num_subpasses_));
  image_usage_history_map_.emplace(image, std::move(history));
  return *this;
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
