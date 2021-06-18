//
//  image_util.cc
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/image_util.h"

#include "lighter/common/util.h"
#include "lighter/renderer/type.h"

namespace lighter::renderer::vk::image {
namespace {

using UsageType = ImageUsage::UsageType;

// Converts 'access_type' to VkAccessFlags, depending on whether it contains
// read and/or write.
intl::AccessFlags GetReadWriteFlags(AccessType access_type,
                                    intl::AccessFlagBits read_flag,
                                    intl::AccessFlagBits write_flag) {
  ASSERT_FALSE(access_type == AccessType::kDontCare,
               "Access type not specified");
  intl::AccessFlags access_flags;
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
intl::ImageUsageFlagBits GetImageUsageFlagBits(const ImageUsage& usage) {
  using UsageFlag = intl::ImageUsageFlagBits;
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
      FATAL("No corresponding image usage flag bits for usage type kDontCare");

    case UsageType::kRenderTarget:
    case UsageType::kMultisampleResolve:
    case UsageType::kPresentation:
      return UsageFlag::eColorAttachment;

    case UsageType::kDepthStencil:
      return UsageFlag::eDepthStencilAttachment;

    case UsageType::kLinearAccess:
      return UsageFlag::eStorage;

    case UsageType::kInputAttachment:
      return UsageFlag::eInputAttachment;

    case UsageType::kSample:
      return UsageFlag::eSampled;

    case UsageType::kTransfer:
      switch (usage.access_type()) {
        case AccessType::kDontCare:
        case AccessType::kReadWrite:
          FATAL("Access type must not be kDontCare or kReadWrite for usage "
                "type kTransfer");

        case AccessType::kReadOnly:
          return UsageFlag::eTransferSrc;

        case AccessType::kWriteOnly:
          return UsageFlag::eTransferDst;
      }
  }
}

}  // namespace

intl::AccessFlags GetAccessFlags(const ImageUsage& usage) {
  using AccessFlag = intl::AccessFlagBits;
  const AccessType access_type = usage.access_type();
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
    case UsageType::kPresentation:
      return {};

    case UsageType::kRenderTarget:
    case UsageType::kMultisampleResolve:
      return GetReadWriteFlags(access_type,
                               AccessFlag::eColorAttachmentRead,
                               AccessFlag::eColorAttachmentWrite);

    case UsageType::kDepthStencil:
      return GetReadWriteFlags(access_type,
                               AccessFlag::eDepthStencilAttachmentRead,
                               AccessFlag::eDepthStencilAttachmentWrite);

    case UsageType::kLinearAccess:
    case UsageType::kSample:
      return usage.access_location() == AccessLocation::kHost
                 ? GetReadWriteFlags(access_type,
                                     AccessFlag::eHostRead,
                                     AccessFlag::eHostWrite)
                 : GetReadWriteFlags(access_type,
                                     AccessFlag::eShaderRead,
                                     AccessFlag::eShaderWrite);

    case UsageType::kInputAttachment:
      return AccessFlag::eInputAttachmentRead;

    case UsageType::kTransfer:
      return GetReadWriteFlags(access_type,
                               AccessFlag::eTransferRead,
                               AccessFlag::eTransferWrite);
  }
}

intl::PipelineStageFlags GetPipelineStageFlags(const ImageUsage& usage) {
  using StageFlag = intl::PipelineStageFlagBits;
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
      return StageFlag::eTopOfPipe;

    case UsageType::kRenderTarget:
    case UsageType::kMultisampleResolve:
    case UsageType::kPresentation:
      return StageFlag::eColorAttachmentOutput;

    case UsageType::kDepthStencil:
      return StageFlag::eEarlyFragmentTests | StageFlag::eLateFragmentTests;

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
          return StageFlag::eHost;

        case AccessLocation::kFragmentShader:
          return StageFlag::eFragmentShader;

        case AccessLocation::kComputeShader:
          return StageFlag::eComputeShader;
      }

    case UsageType::kTransfer:
      return StageFlag::eTransfer;
  }
}

intl::ImageLayout GetImageLayout(const ImageUsage& usage) {
  using ImageLayout = intl::ImageLayout;
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
      return ImageLayout::eUndefined;

    case UsageType::kRenderTarget:
    case UsageType::kMultisampleResolve:
      return ImageLayout::eColorAttachmentOptimal;

    case UsageType::kDepthStencil:
      return ImageLayout::eDepthStencilAttachmentOptimal;

    case UsageType::kPresentation:
      return ImageLayout::ePresentSrcKHR;

    case UsageType::kLinearAccess:
      return ImageLayout::eGeneral;

    case UsageType::kInputAttachment:
    case UsageType::kSample:
      return ImageLayout::eShaderReadOnlyOptimal;

    case UsageType::kTransfer:
      switch (usage.access_type()) {
        case AccessType::kDontCare:
        case AccessType::kReadWrite:
          FATAL("Access type must not be kDontCare or kReadWrite for usage "
                "type kTransfer");

        case AccessType::kReadOnly:
          return ImageLayout::eTransferSrcOptimal;

        case AccessType::kWriteOnly:
          return ImageLayout::eTransferDstOptimal;
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

intl::ImageUsageFlags GetImageUsageFlags(absl::Span<const ImageUsage> usages) {
  intl::ImageUsageFlags flags;
  for (const auto& usage : usages) {
    if (usage.usage_type() != UsageType::kDontCare) {
      flags |= GetImageUsageFlagBits(usage);
    }
  }
  return flags;
}

bool NeedsSynchronization(const ImageUsage& prev_usage,
                          const ImageUsage& curr_usage) {
  // RAR.
  if (curr_usage == prev_usage &&
      curr_usage.access_type() == AccessType::kReadOnly) {
    return false;
  }
  return true;
}

}  // namespace lighter::renderer::vk::image
