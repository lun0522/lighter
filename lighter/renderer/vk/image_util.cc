//
//  image_util.cc
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/image_util.h"

#include "lighter/common/util.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk::image {
namespace {

using UsageType = ImageUsage::UsageType;

constexpr VkAccessFlags kNullAccessFlag = 0;

// Converts 'access_type' to VkAccessFlags, depending on whether it contains
// read and/or write.
VkAccessFlags GetReadWriteFlags(AccessType access_type,
                                VkAccessFlagBits read_flag,
                                VkAccessFlagBits write_flag) {
  ASSERT_FALSE(access_type == AccessType::kDontCare,
               "Access type not specified");
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

// Returns VkImageUsageFlagBits for 'usage'. Note that this must not be called
// if usage type is kDontCare, since it doesn't have corresponding flag bits.
VkImageUsageFlagBits GetImageUsageFlagBits(const ImageUsage& usage) {
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
      FATAL("No corresponding image usage flag bits for usage type kDontCare");

    case UsageType::kRenderTarget:
    case UsageType::kMultisampleResolve:
    case UsageType::kPresentation:
      return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    case UsageType::kDepthStencil:
      return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    case UsageType::kLinearAccess:
      return VK_IMAGE_USAGE_STORAGE_BIT;

    case UsageType::kInputAttachment:
      return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    case UsageType::kSample:
      return VK_IMAGE_USAGE_SAMPLED_BIT;

    case UsageType::kTransfer:
      switch (usage.access_type()) {
        case AccessType::kDontCare:
        case AccessType::kReadWrite:
          FATAL("Access type must not be kDontCare or kReadWrite for usage "
                "type kTransfer");

        case AccessType::kReadOnly:
          return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        case AccessType::kWriteOnly:
          return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      }
  }
}

}  // namespace

VkAccessFlags GetAccessFlags(const ImageUsage& usage) {
  const AccessType access_type = usage.access_type();
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
    case UsageType::kPresentation:
      return kNullAccessFlag;

    case UsageType::kRenderTarget:
    case UsageType::kMultisampleResolve:
      return GetReadWriteFlags(access_type,
                               VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    case UsageType::kDepthStencil:
      return GetReadWriteFlags(access_type,
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    case UsageType::kLinearAccess:
    case UsageType::kSample:
      return usage.access_location() == AccessLocation::kHost
                 ? GetReadWriteFlags(access_type,
                                     VK_ACCESS_HOST_READ_BIT,
                                     VK_ACCESS_HOST_WRITE_BIT)
                 : GetReadWriteFlags(access_type,
                                     VK_ACCESS_SHADER_READ_BIT,
                                     VK_ACCESS_SHADER_WRITE_BIT);

    case UsageType::kInputAttachment:
      return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

    case UsageType::kTransfer:
      return GetReadWriteFlags(access_type,
                               VK_ACCESS_TRANSFER_READ_BIT,
                               VK_ACCESS_TRANSFER_WRITE_BIT);
  }
}

VkPipelineStageFlags GetPipelineStageFlags(const ImageUsage& usage) {
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
      return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    case UsageType::kRenderTarget:
    case UsageType::kMultisampleResolve:
    case UsageType::kPresentation:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    case UsageType::kDepthStencil:
      return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                 | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    case UsageType::kLinearAccess:
    case UsageType::kInputAttachment:
    case UsageType::kSample:
      switch (usage.access_location()) {
        case AccessLocation::kDontCare:
        case AccessLocation::kVertexShader:
        case AccessLocation::kOther:
          FATAL("Access location must not be kDontCare, kVertexShader or "
                "kOther for usage type kLinearAccess and kSample");

        case AccessLocation::kHost:
          return VK_PIPELINE_STAGE_HOST_BIT;

        case AccessLocation::kFragmentShader:
          return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        case AccessLocation::kComputeShader:
          return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      }

    case UsageType::kTransfer:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
}

VkImageLayout GetImageLayout(const ImageUsage& usage) {
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
      return VK_IMAGE_LAYOUT_UNDEFINED;

    case UsageType::kRenderTarget:
    case UsageType::kMultisampleResolve:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    case UsageType::kDepthStencil:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    case UsageType::kPresentation:
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    case UsageType::kLinearAccess:
      return VK_IMAGE_LAYOUT_GENERAL;

    case UsageType::kInputAttachment:
    case UsageType::kSample:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    case UsageType::kTransfer:
      switch (usage.access_type()) {
        case AccessType::kDontCare:
        case AccessType::kReadWrite:
          FATAL("Access type must not be kDontCare or kReadWrite for usage "
                "type kTransfer");

        case AccessType::kReadOnly:
          return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        case AccessType::kWriteOnly:
          return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      }
  }
}

std::optional<uint32_t> GetQueueFamilyIndex(const Context& context,
                                            const ImageUsage& usage) {
  const auto& queue_family_indices =
      context.physical_device().queue_family_indices();
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
    case UsageType::kPresentation:
    case UsageType::kTransfer:
      return std::nullopt;

    case UsageType::kRenderTarget:
    case UsageType::kDepthStencil:
    case UsageType::kMultisampleResolve:
      return queue_family_indices.graphics;

    case UsageType::kLinearAccess:
    case UsageType::kInputAttachment:
    case UsageType::kSample:
      switch (usage.access_location()) {
        case AccessLocation::kDontCare:
        case AccessLocation::kVertexShader:
        case AccessLocation::kOther:
          FATAL("Access location must not be kDontCare, kVertexShader or "
                "kOther for usage type kLinearAccess and kSample");

        case AccessLocation::kHost:
          return std::nullopt;

        case AccessLocation::kFragmentShader:
          return queue_family_indices.graphics;

        case AccessLocation::kComputeShader:
          return queue_family_indices.compute;
      }
  }
}

VkImageUsageFlags GetImageUsageFlags(absl::Span<const ImageUsage> usages) {
  auto flags = nullflag;
  for (const auto& usage : usages) {
    if (usage.usage_type() != UsageType::kDontCare) {
      flags |= GetImageUsageFlagBits(usage);
    }
  }
  return static_cast<VkImageUsageFlags>(flags);
}

bool NeedSynchronization(const ImageUsage& prev_usage,
                         const ImageUsage& curr_usage) {
  // RAR.
  if (curr_usage == prev_usage &&
      curr_usage.access_type() == AccessType::kReadOnly) {
    return false;
  }
  return true;
}

}  // namespace lighter::renderer::vk::image
