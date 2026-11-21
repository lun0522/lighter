//
//  image_util.cc
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/image_util.h"

#include "lighter/common/util.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::renderer::vk::image {
namespace {

using ir::AccessLocation;
using ir::AccessType;
using ir::ImageUsage;
using UsageType = ImageUsage::UsageType;

// Converts 'access_type' to VkAccessFlags2, depending on whether it contains
// read and/or write.
intl::AccessFlags2 GetReadWriteFlags(AccessType access_type,
                                     intl::AccessFlagBits2 read_flag,
                                     intl::AccessFlagBits2 write_flag) {
  ASSERT_FALSE(access_type == AccessType::kDontCare,
               "Access type not specified");
  intl::AccessFlags2 access_flags;
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

// Masks out read-only bits so srcAccessMask only flushes prior writes.
intl::AccessFlags2 GetWriteOnlyAccessFlags(const ImageUsage& usage) {
  const AccessType access_type = usage.access_type();
  if (access_type != AccessType::kWriteOnly &&
      access_type != AccessType::kReadWrite) {
    return {};
  }
  constexpr intl::AccessFlags2 kWriteAccessMask =
      intl::AccessFlagBits2::eShaderWrite |
      intl::AccessFlagBits2::eColorAttachmentWrite |
      intl::AccessFlagBits2::eDepthStencilAttachmentWrite |
      intl::AccessFlagBits2::eTransferWrite |
      intl::AccessFlagBits2::eHostWrite | intl::AccessFlagBits2::eMemoryWrite;
  return GetAccessFlags(usage) & kWriteAccessMask;
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
          FATAL(
              "Access type must not be kDontCare or kReadWrite for usage "
              "type kTransfer");

        case AccessType::kReadOnly:
          return UsageFlag::eTransferSrc;

        case AccessType::kWriteOnly:
          return UsageFlag::eTransferDst;
      }
  }
}

}  // namespace

intl::AccessFlags2 GetAccessFlags(const ImageUsage& usage) {
  using AccessFlag = intl::AccessFlagBits2;
  const AccessType access_type = usage.access_type();
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
    case UsageType::kPresentation:
      return {};

    case UsageType::kRenderTarget:
    case UsageType::kMultisampleResolve:
      return GetReadWriteFlags(access_type, AccessFlag::eColorAttachmentRead,
                               AccessFlag::eColorAttachmentWrite);

    case UsageType::kDepthStencil:
      return GetReadWriteFlags(access_type,
                               AccessFlag::eDepthStencilAttachmentRead,
                               AccessFlag::eDepthStencilAttachmentWrite);

    case UsageType::kLinearAccess:
    case UsageType::kSample:
      return usage.access_location() == AccessLocation::kHost
                 ? GetReadWriteFlags(access_type, AccessFlag::eHostRead,
                                     AccessFlag::eHostWrite)
                 : GetReadWriteFlags(access_type, AccessFlag::eShaderRead,
                                     AccessFlag::eShaderWrite);

    case UsageType::kInputAttachment:
      return AccessFlag::eInputAttachmentRead;

    case UsageType::kTransfer:
      return GetReadWriteFlags(access_type, AccessFlag::eTransferRead,
                               AccessFlag::eTransferWrite);
  }
}

intl::PipelineStageFlags2 GetPipelineStageFlags(const ImageUsage& usage) {
  using StageFlag = intl::PipelineStageFlagBits2;
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
    case UsageType::kPresentation:
      return StageFlag::eNone;

    case UsageType::kRenderTarget:
    case UsageType::kMultisampleResolve:
      return StageFlag::eColorAttachmentOutput;

    case UsageType::kDepthStencil:
      return StageFlag::eEarlyFragmentTests | StageFlag::eLateFragmentTests;

    case UsageType::kLinearAccess:
    case UsageType::kInputAttachment:
    case UsageType::kSample:
      switch (usage.access_location()) {
        case AccessLocation::kDontCare:
        case AccessLocation::kOther:
          FATAL(
              "Access location must not be kDontCare or kOther for usage "
              "type kLinearAccess and kSample");

        case AccessLocation::kHost:
          return StageFlag::eHost;

        case AccessLocation::kVertexShader:
          return StageFlag::eVertexShader;

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
      return usage.access_type() == AccessType::kReadOnly
                 ? ImageLayout::eDepthStencilReadOnlyOptimal
                 : ImageLayout::eDepthStencilAttachmentOptimal;

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
          FATAL(
              "Access type must not be kDontCare or kReadWrite for usage "
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
        case AccessLocation::kOther:
          FATAL(
              "Access location must not be kDontCare or kOther for usage "
              "type kLinearAccess and kSample");

        case AccessLocation::kHost:
          return std::nullopt;

        case AccessLocation::kVertexShader:
        case AccessLocation::kFragmentShader:
          return queue_family_indices.graphics;

        case AccessLocation::kComputeShader:
          return queue_family_indices.compute;
      }
  }
}

intl::ImageUsageFlags GetImageUsageFlags(std::span<const ImageUsage> usages) {
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
  if (curr_usage.usage_type() == UsageType::kDontCare) {
    return false;
  }
  const intl::ImageLayout prev_layout = GetImageLayout(prev_usage);
  const intl::ImageLayout curr_layout = GetImageLayout(curr_usage);
  // Consecutive reads in the same non-undefined/non-present layout need no
  // barrier.
  if (prev_layout == curr_layout &&
      prev_usage.usage_type() != UsageType::kDontCare &&
      prev_usage.usage_type() != UsageType::kPresentation &&
      prev_usage.access_type() == AccessType::kReadOnly &&
      curr_usage.access_type() == AccessType::kReadOnly) {
    return false;
  }
  return true;
}

std::optional<intl::ImageMemoryBarrier2> CreateImageMemoryBarrier(
    intl::Image image, const intl::ImageSubresourceRange& subresource_range,
    const ir::ImageUsage& prev_usage, const ir::ImageUsage& curr_usage,
    uint32_t src_queue_family_index, uint32_t dst_queue_family_index) {
  if (!NeedsSynchronization(prev_usage, curr_usage)) {
    return std::nullopt;
  }
  return intl::ImageMemoryBarrier2{}
      .setSrcStageMask(GetPipelineStageFlags(prev_usage))
      .setSrcAccessMask(GetWriteOnlyAccessFlags(prev_usage))
      .setDstStageMask(GetPipelineStageFlags(curr_usage))
      .setDstAccessMask(GetAccessFlags(curr_usage))
      .setOldLayout(GetImageLayout(prev_usage))
      .setNewLayout(GetImageLayout(curr_usage))
      .setSrcQueueFamilyIndex(src_queue_family_index)
      .setDstQueueFamilyIndex(dst_queue_family_index)
      .setImage(image)
      .setSubresourceRange(subresource_range);
}

intl::ImageView CreateImageView(const Context& context, intl::Image vk_image,
                                intl::ImageViewType view_type,
                                intl::Format format,
                                intl::ImageAspectFlags aspect_mask,
                                uint32_t mip_levels, uint32_t layer_count) {
  const auto subresource_range =
      intl::ImageSubresourceRange{}
          .setAspectMask(aspect_mask)
          .setLevelCount(mip_levels)
          .setLayerCount(layer_count);
  const auto create_info = intl::ImageViewCreateInfo{}
                               .setImage(vk_image)
                               .setViewType(view_type)
                               .setFormat(format)
                               .setSubresourceRange(subresource_range);
  return context.device()->createImageView(create_info,
                                           *context.host_allocator());
}

void TransitionImageLayout(intl::CommandBuffer cmd, intl::Image image,
                           intl::ImageAspectFlags aspect_mask,
                           uint32_t base_mip_level, uint32_t level_count,
                           uint32_t layer_count, intl::ImageLayout old_layout,
                           intl::ImageLayout new_layout,
                           intl::PipelineStageFlags2 src_stage_mask,
                           intl::AccessFlags2 src_access_mask,
                           intl::PipelineStageFlags2 dst_stage_mask,
                           intl::AccessFlags2 dst_access_mask) {
  const auto barrier =
      intl::ImageMemoryBarrier2{}
          .setSrcStageMask(src_stage_mask)
          .setSrcAccessMask(src_access_mask)
          .setDstStageMask(dst_stage_mask)
          .setDstAccessMask(dst_access_mask)
          .setOldLayout(old_layout)
          .setNewLayout(new_layout)
          .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
          .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
          .setImage(image)
          .setSubresourceRange({aspect_mask, base_mip_level, level_count,
                                /*baseArrayLayer=*/0, layer_count});
  cmd.pipelineBarrier2(intl::DependencyInfo{}.setImageMemoryBarriers(barrier));
}

}  // namespace lighter::renderer::vk::image
