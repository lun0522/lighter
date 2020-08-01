//
//  naive_render_pass.cc
//
//  Created by Pujun Lun on 7/31/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/naive_render_pass.h"

#include <string>

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/strings/string_view.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

void CheckSubpassIndexInRange(int subpass, int num_subpasses,
                              absl::string_view name) {
  ASSERT_TRUE(0 <= subpass && subpass < num_subpasses,
              absl::StrFormat("First %s subpass index (%d) must be in range "
                              "[0, %d)",
                              name, subpass, num_subpasses));
}

void PrintSubpassCountIfNonZero(int count, absl::string_view name) {
  if (count > 0) {
    LOG_INFO << absl::StreamFormat("Number of %s subpasses: %d", name, count);
  }
}

} /* namespace */

NaiveRenderPass::SubpassConfig::SubpassConfig(
    int num_subpasses, absl::optional<int> first_transparent_subpass,
    absl::optional<int> first_overlay_subpass) {
  ASSERT_TRUE(num_subpasses >= 0,
              absl::StrFormat("Number of subpasses (%d) must be positive",
                              num_subpasses));
  if (first_transparent_subpass.has_value()) {
    CheckSubpassIndexInRange(first_transparent_subpass.value(), num_subpasses,
                             "transparent");
  }
  if (first_overlay_subpass.has_value()) {
    CheckSubpassIndexInRange(first_overlay_subpass.value(), num_subpasses,
                             "overlay");
  }

  if (first_overlay_subpass.has_value()) {
    num_overlay_subpasses_ = num_subpasses - first_overlay_subpass.value();
  }

  if (first_transparent_subpass.has_value()) {
    num_opaque_subpass_ = first_transparent_subpass.value();
    num_transparent_subpasses_ = num_subpasses - num_opaque_subpass_ -
                                 num_overlay_subpasses_;
  } else {
    num_opaque_subpass_ = num_subpasses - num_overlay_subpasses_;
  }
}

std::unique_ptr<RenderPassBuilder> NaiveRenderPass::CreateBuilder(
    SharedBasicContext context, int num_framebuffers,
    const SubpassConfig& subpass_config) {
#ifndef NDEBUG
  LOG_INFO << "Building naive render pass:";
  PrintSubpassCountIfNonZero(subpass_config.num_opaque_subpass_, "opaque");
  PrintSubpassCountIfNonZero(subpass_config.num_transparent_subpasses_,
                             "transparent");
  PrintSubpassCountIfNonZero(subpass_config.num_overlay_subpasses_, "overlay");
#endif  /* !NDEBUG */

  GraphicsPass graphics_pass{std::move(context),
                             subpass_config.num_subpasses()};
  return graphics_pass.CreateRenderPassBuilder(num_framebuffers);
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
