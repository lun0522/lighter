//
//  compute_pass.cc
//
//  Created by Pujun Lun on 6/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/compute_pass.h"

#include <iterator>
#include <string>

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/wrapper/image_util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {

ComputePass& ComputePass::AddImage(std::string&& image_name,
                                   ImageUsageHistory&& history) {
  ValidateUsageHistory(image_name, history);
  BasePass::AddUsageHistory(std::move(image_name), std::move(history));
  return *this;
}

void ComputePass::Run(
    const VkCommandBuffer& command_buffer, uint32_t queue_family_index,
    const absl::flat_hash_map<std::string, const Image*>& image_map,
    absl::Span<const ComputeOp> compute_ops) const {
  ASSERT_TRUE(compute_ops.size() == num_subpasses_,
              absl::StrFormat("Size of 'compute_ops' (%d) mismatches with the "
                              "number of subpasses (%d)",
                              compute_ops.size(), num_subpasses_));

  // Run all subpasses and insert memory barriers. Note that even if the image
  // usage does not change, we still need to insert a memory barrier if not RAR.
  ASSERT_TRUE(virtual_final_subpass_index() == num_subpasses_,
              "Assumption of the following loop is broken");
  for (int subpass = 0; subpass <= num_subpasses_; ++subpass) {
    for (const auto& pair : image_usage_history_map()) {
      const std::string& image_name = pair.first;
      const auto usages_info =
          GetImageUsagesIfNeedSynchronization(image_name, subpass);
      if (!usages_info.has_value()) {
        continue;
      }

      const auto iter = image_map.find(image_name);
      ASSERT_FALSE(iter == image_map.end(),
                   absl::StrFormat("Image '%s' not provided in image map",
                                   image_name));
      InsertMemoryBarrier(command_buffer, queue_family_index, **iter->second,
                          usages_info.value().prev_usage,
                          usages_info.value().curr_usage);

#ifndef NDEBUG
      const std::string log_suffix =
          subpass == virtual_final_subpass_index()
              ? "after compute pass"
              : absl::StrFormat("before subpass %d", subpass);
      LOG_INFO << absl::StreamFormat("Inserted memory barrier for image '%s' ",
                                     image_name)
               << log_suffix;
#endif /* !NDEBUG */
    }

    if (subpass < num_subpasses_) {
      compute_ops[subpass]();
    }
  }
}

void ComputePass::InsertMemoryBarrier(
    const VkCommandBuffer& command_buffer, uint32_t queue_family_index,
    const VkImage& image, const ImageUsage& prev_usage,
    const ImageUsage& curr_usage) const {
  const VkImageMemoryBarrier barrier{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      /*pNext=*/nullptr,
      /*srcAccessMask=*/image::GetAccessFlags(prev_usage),
      /*dstAccessMask=*/image::GetAccessFlags(curr_usage),
      /*oldLayout=*/image::GetImageLayout(prev_usage),
      /*newLayout=*/image::GetImageLayout(curr_usage),
      /*srcQueueFamilyIndex=*/queue_family_index,
      /*dstQueueFamilyIndex=*/queue_family_index,
      image,
      VkImageSubresourceRange{
          VK_IMAGE_ASPECT_COLOR_BIT,
          /*baseMipLevel=*/0,
          /*levelCount=*/1,
          /*baseArrayLayer=*/0,
          /*layerCount=*/1,
      },
  };

  vkCmdPipelineBarrier(
      command_buffer,
      /*srcStageMask=*/image::GetPipelineStageFlags(prev_usage),
      /*dstStageMask=*/image::GetPipelineStageFlags(curr_usage),
      /*dependencyFlags=*/0,
      /*memoryBarrierCount=*/0,
      /*pMemoryBarriers=*/nullptr,
      /*bufferMemoryBarrierCount=*/0,
      /*pBufferMemoryBarriers=*/nullptr,
      /*imageMemoryBarrierCount=*/1,
      &barrier);
}

void ComputePass::ValidateUsageHistory(
    const std::string& image_name, const ImageUsageHistory& history) const {
  using ImageUsageType = ImageUsage::UsageType;
  for (const auto& pair : history.usage_at_subpass_map()) {
    const ImageUsageType usage_type = pair.second.usage_type();
    ASSERT_TRUE(usage_type == ImageUsageType::kLinearAccess
                    || usage_type == ImageUsageType::kSample
                    || usage_type == ImageUsageType::kTransfer,
                absl::StrFormat("Usage type (%d) is neither kLinearAccess, "
                                "kSample nor kTransfer for image '%s' at "
                                "subpass %d",
                                usage_type, image_name, pair.first));
  }
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
