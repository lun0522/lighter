//
//  naive_render_pass.cc
//
//  Created by Pujun Lun on 7/31/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/naive_render_pass.h"

#include <string>

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
    const SubpassConfig& subpass_config,
    const AttachmentConfig& color_attachment_config,
    const AttachmentConfig* multisampling_attachment_config,
    const AttachmentConfig* depth_stencil_attachment_config,
    image::UsageTracker& image_usage_tracker) {
#ifndef NDEBUG
  LOG_INFO << "Building naive render pass";
  PrintSubpassCountIfNonZero(subpass_config.num_opaque_subpass_, "opaque");
  PrintSubpassCountIfNonZero(subpass_config.num_transparent_subpasses_,
                             "transparent");
  PrintSubpassCountIfNonZero(subpass_config.num_overlay_subpasses_, "overlay");
#endif  /* !NDEBUG */

  const int num_subpasses = subpass_config.num_subpasses();
  const int first_subpass = 0;
  const int last_subpass = num_subpasses - 1;

  const auto get_location = [](int subpass) { return 0; };
  bool use_multisampling = multisampling_attachment_config != nullptr;
  bool use_depth_stencil = depth_stencil_attachment_config != nullptr;
  const int num_subpasses_using_depth_stencil =
      subpass_config.num_subpasses_using_depth_stencil() > 0;

  if (use_depth_stencil) {
    ASSERT_TRUE(num_subpasses_using_depth_stencil > 0,
                "Depth stencil attachment config is provided, but this "
                "attachment is never used");
  } else {
    ASSERT_TRUE(num_subpasses_using_depth_stencil == 0,
                "Depth stencil attachment is used in subpasses, but no config "
                "is provided");
  }

  GraphicsPass graphics_pass{std::move(context), num_subpasses};

  color_attachment_config.attachment_info.AddToGraphicsPass(
      graphics_pass, image_usage_tracker, get_location,
      /*populate_history=*/[&](image::UsageHistory& history) {
        if (use_multisampling) {
          history.AddUsage(last_subpass,
                           image::Usage::GetMultisampleResolveTargetUsage());
        } else {
          history.AddUsage(first_subpass, last_subpass,
                           image::Usage::GetRenderTargetUsage());
        }

        if (color_attachment_config.final_usage.has_value()) {
          history.SetFinalUsage(color_attachment_config.final_usage.value());
        }
      },
      color_attachment_config.load_store_ops);

  if (use_multisampling) {
    const AttachmentInfo& resolve_target =
        color_attachment_config.attachment_info;
    multisampling_attachment_config->attachment_info
        .AddToGraphicsPass(
            graphics_pass, image_usage_tracker, get_location,
            /*populate_history=*/[&](image::UsageHistory& history) {
              history.AddUsage(first_subpass, last_subpass,
                               image::Usage::GetRenderTargetUsage());

              if (multisampling_attachment_config->final_usage.has_value()) {
                history.SetFinalUsage(
                    multisampling_attachment_config->final_usage.value());
              }
            },
            multisampling_attachment_config->load_store_ops)
        .ResolveToAttachment(graphics_pass, resolve_target, last_subpass);
  }

  if (use_depth_stencil) {
    depth_stencil_attachment_config->attachment_info.AddToGraphicsPass(
        graphics_pass, image_usage_tracker, get_location,
        /*populate_history=*/[&](image::UsageHistory& history) {
          if (subpass_config.num_opaque_subpass_ > 0) {
            const int last_opaque_subpass =
                subpass_config.num_opaque_subpass_ - 1;
            history.AddUsage(first_subpass, last_opaque_subpass,
                             image::Usage::GetDepthStencilUsage(
                                 image::Usage::AccessType::kReadWrite));
          }

          if (subpass_config.num_transparent_subpasses_ > 0) {
            const int first_transparent_subpass =
                subpass_config.num_opaque_subpass_;
            const int last_transparent_subpass =
                first_transparent_subpass +
                subpass_config.num_transparent_subpasses_ - 1;
            history.AddUsage(first_transparent_subpass,
                             last_transparent_subpass,
                             image::Usage::GetDepthStencilUsage(
                                 image::Usage::AccessType::kReadOnly));
          }

          if (depth_stencil_attachment_config->final_usage.has_value()) {
            history.SetFinalUsage(
                depth_stencil_attachment_config->final_usage.value());
          }
        },
        depth_stencil_attachment_config->load_store_ops);
  }

  return graphics_pass.CreateRenderPassBuilder(num_framebuffers);
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */