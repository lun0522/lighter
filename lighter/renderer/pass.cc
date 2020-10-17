//
//  pass.cc
//
//  Created by Pujun Lun on 10/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/pass.h"

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {

void BasePassDescriptor::AddImage(absl::string_view name,
                                  ImageUsageHistory&& usage_history) {
  for (const auto& pair : usage_history.usage_at_subpass_map()) {
    ValidateSubpass(/*subpass=*/pair.first, name,
                    /*include_virtual_subpasses=*/false);
  }

  usage_history.AddUsage(virtual_initial_subpass_index(),
                         usage_history.initial_usage());
  if (usage_history.final_usage().has_value()) {
    usage_history.AddUsage(virtual_final_subpass_index(),
                           usage_history.final_usage().value());
  }
  image_usage_history_map_.emplace(name, std::move(usage_history));
}

void BasePassDescriptor::ValidateSubpass(int subpass,
                                         absl::string_view image_name,
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

void GraphicsPassDescriptor::ValidateUsages(
    absl::string_view image_name, const ImageUsageHistory& usage_history,
    AttachmentType attachment_type) const {
  bool is_attachment_used = false;
  for (const auto& pair : usage_history.usage_at_subpass_map()) {
    if (IsVirtualSubpass(pair.first)) {
      continue;
    }

    const ImageUsage::UsageType usage_type = pair.second.usage_type();
    if (usage_type == ImageUsage::UsageType::kDontCare) {
      continue;
    }

    const int subpass = pair.first;
    switch (attachment_type) {
      case AttachmentType::kColor:
        ASSERT_TRUE(
            usage_type == ImageUsage::UsageType::kRenderTarget
                || usage_type == ImageUsage::UsageType::kMultisampleResolve,
            absl::StrFormat("Unexpected usage type (%d) for color attachment "
                            "'%s' at subpass %d",
                            usage_type, image_name, subpass));
        break;

      case AttachmentType::kDepthStencil:
        ASSERT_TRUE(usage_type == ImageUsage::UsageType::kDepthStencil,
                    absl::StrFormat("Unexpected usage type (%d) for depth "
                                    "stencil attachment '%s' at subpass %d",
                                    usage_type, image_name, subpass));
        break;
    }
  }

  ASSERT_TRUE(is_attachment_used,
              absl::StrFormat("Image '%s' is not used in the graphics pass",
                              image_name));
}

} /* namespace renderer */
} /* namespace lighter */
