//
//  image_util.cc
//
//  Created by Pujun Lun on 4/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/image_util.h"

#include <algorithm>

#include "lighter/renderer/vulkan/wrapper/util.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

using UsageType = image::Usage::UsageType;
using AccessType = image::Usage::AccessType;

// Converts 'access_type' to VkAccessFlags, depending on whether it contains
// read and/or write.
VkAccessFlags GetReadWriteFlags(AccessType access_type,
                                VkAccessFlagBits read_flag,
                                VkAccessFlagBits write_flag) {
  VkAccessFlags access_flags = kNullAccessFlag;
  if (access_type == AccessType::kReadOnly ||
      access_type == AccessType::kReadWrite) {
    access_flags |= read_flag;
  }
  if (access_type == AccessType::kWriteOnly ||
      access_type == AccessType::kReadWrite) {
    access_flags |= write_flag;
  }
  return access_flags;
}

} /* namespace */

namespace image {

VkAccessFlags Usage::GetAccessFlags() const {
  switch (usage_type_) {
    case UsageType::kDontCare:
      return kNullAccessFlag;

    case UsageType::kRenderTarget:
      return GetReadWriteFlags(access_type_,
                               VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    case UsageType::kDepthStencil:
      return GetReadWriteFlags(access_type_,
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    case UsageType::kPresentation:
      FATAL("No corresponding access flags for UsageType::kPresentation");

    case UsageType::kLinearAccess:
    case UsageType::kSample:
      return access_location_ == AccessLocation::kHost
                 ? GetReadWriteFlags(access_type_,
                                     VK_ACCESS_HOST_READ_BIT,
                                     VK_ACCESS_HOST_WRITE_BIT)
                 : GetReadWriteFlags(access_type_,
                                     VK_ACCESS_SHADER_READ_BIT,
                                     VK_ACCESS_SHADER_WRITE_BIT);

    case UsageType::kTransfer:
      return GetReadWriteFlags(access_type_,
                               VK_ACCESS_TRANSFER_READ_BIT,
                               VK_ACCESS_TRANSFER_WRITE_BIT);
  }
}

VkPipelineStageFlags Usage::GetPipelineStageFlags() const {
  switch (usage_type_) {
    case UsageType::kDontCare:
      return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    case UsageType::kRenderTarget:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    case UsageType::kDepthStencil:
      return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                 | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    case UsageType::kPresentation:
      FATAL("No corresponding pipeline stage flags for "
            "UsageType::kPresentation");

    case UsageType::kLinearAccess:
    case UsageType::kSample:
      switch (access_location_) {
        case AccessLocation::kDontCare:
          FATAL("Access location must be specified for "
                "UsageType::kLinearAccess and UsageType::kSample");

        case AccessLocation::kHost:
          return VK_PIPELINE_STAGE_HOST_BIT;

        case AccessLocation::kFragmentShader:
          return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        case AccessLocation::kComputeShader:
          return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        case AccessLocation::kOther:
          FATAL("Access location must not be kOther for"
                "UsageType::kLinearAccess and UsageType::kSample");
      }

    case UsageType::kTransfer:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
}

VkImageLayout Usage::GetImageLayout() const {
  switch (usage_type_) {
    case UsageType::kDontCare:
      return VK_IMAGE_LAYOUT_UNDEFINED;

    case UsageType::kRenderTarget:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    case UsageType::kDepthStencil:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    case UsageType::kPresentation:
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    case UsageType::kLinearAccess:
      return VK_IMAGE_LAYOUT_GENERAL;

    case UsageType::kSample:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    case UsageType::kTransfer:
      switch (access_type_) {
        case AccessType::kDontCare:
          FATAL("Access type not specified for UsageType::kTransfer");

        case AccessType::kReadOnly:
          return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        case AccessType::kWriteOnly:
          return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        case AccessType::kReadWrite:
          FATAL("Access type must not be kReadWrite for UsageType::kTransfer");
      }
  }
}

VkImageUsageFlagBits Usage::GetImageUsageFlagBits() const {
  switch (usage_type_) {
    case UsageType::kDontCare:
      FATAL("No corresponding image usage flag bits for UsageType::kDontCare");

    case UsageType::kRenderTarget:
    case UsageType::kPresentation:
      return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    case UsageType::kDepthStencil:
      return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    case UsageType::kLinearAccess:
      return VK_IMAGE_USAGE_STORAGE_BIT;

    case UsageType::kSample:
      return VK_IMAGE_USAGE_SAMPLED_BIT;

    case UsageType::kTransfer:
      switch (access_type_) {
        case AccessType::kDontCare:
          FATAL("Access type not specified for UsageType::kTransfer");

        case AccessType::kReadOnly:
          return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        case AccessType::kWriteOnly:
          return VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        case AccessType::kReadWrite:
          FATAL("Access type must not be kReadWrite for UsageType::kTransfer");
      }
  }
}

bool IsLinearAccessed(absl::Span<const Usage> usages) {
  return std::any_of(usages.begin(), usages.end(), [](const Usage& usage) {
    return usage.usage_type() == UsageType::kLinearAccess;
  });
}

bool UseHighPrecision(absl::Span<const Usage> usages) {
  return std::any_of(usages.begin(), usages.end(), [](const Usage& usage) {
    return usage.use_high_precision();
  });
}

VkImageUsageFlags GetImageUsageFlags(absl::Span<const Usage> usages) {
  auto flags = nullflag;
  for (const auto& usage : usages) {
    if (usage.usage_type() != UsageType::kDontCare) {
      flags |= usage.GetImageUsageFlagBits();
    }
  }
  return static_cast<VkImageUsageFlags>(flags);
}

bool NeedSynchronization(const Usage& prev_usage, const Usage& curr_usage) {
  if (curr_usage == prev_usage &&
      curr_usage.access_type() == AccessType::kReadOnly) {
    return false;
  }
  return true;
}

} /* namespace image */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */