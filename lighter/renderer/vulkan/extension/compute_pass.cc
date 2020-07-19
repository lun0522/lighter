//
//  compute_pass.cc
//
//  Created by Pujun Lun on 6/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/compute_pass.h"

#include <iterator>

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {

void ComputePass::Run(const VkCommandBuffer& command_buffer,
                      uint32_t queue_family_index,
                      absl::Span<const ComputeOp> compute_ops) const {
  ASSERT_TRUE(compute_ops.size() == num_subpasses_,
              absl::StrFormat("Size of 'compute_ops' (%d) mismatches with the "
                              "number of subpasses (%d)",
                              compute_ops.size(), num_subpasses_));

  // Run all subpasses and insert memory barriers. Note that even if the image
  // usage does not change, we still need to insert a memory barrier if not RAR.
  for (int subpass = 0; subpass < num_subpasses_; ++subpass) {
    for (const auto& pair : image_usage_history_map()) {
      const auto& usage_at_subpass_map = pair.second.usage_at_subpass_map();
      const auto iter = usage_at_subpass_map.find(subpass);
      if (iter == usage_at_subpass_map.end()) {
        continue;
      }

      const image::Usage& next_usage = iter->second;
      const image::Usage& prev_usage = std::prev(iter)->second;
      if (next_usage == prev_usage &&
          next_usage.access_type() == image::Usage::AccessType::kReadOnly) {
        continue;
      }

      InsertMemoryBarrier(command_buffer, queue_family_index, **pair.first,
                          prev_usage, next_usage);
#ifndef NDEBUG
      LOG_INFO << absl::StreamFormat("Inserted memory barrier for image '%s' "
                                     "before subpass %d",
                                     pair.second.image_name(), subpass);
#endif /* !NDEBUG */
    }
    compute_ops[subpass]();
  }

  // For each image, inform Image class the last known usage, and transition the
  // layout if necessary. Note that if the final usage is the same as the
  // previous usage, there is no need to insert a memory barrier.
  for (const auto& pair : image_usage_history_map()) {
    Image& image = *pair.first;
    const image::UsageHistory& history = pair.second;
    const image::Usage& prev_usage =
        history.usage_at_subpass_map().rbegin()->second;
    if (!history.final_usage().has_value()) {
      image.set_usage(prev_usage);
      continue;
    }

    const image::Usage& final_usage = history.final_usage().value();
    image.set_usage(final_usage);
    if (final_usage == prev_usage) {
      continue;
    }

    InsertMemoryBarrier(command_buffer, queue_family_index, **pair.first,
                        prev_usage, final_usage);
#ifndef NDEBUG
    LOG_INFO << absl::StreamFormat("Inserted memory barrier for image '%s' "
                                   "after compute pass",
                                   pair.second.image_name());
#endif /* !NDEBUG */
  }
}

void ComputePass::InsertMemoryBarrier(
    const VkCommandBuffer& command_buffer, uint32_t queue_family_index,
    const VkImage& image, image::Usage prev_usage,
    image::Usage next_usage) const {
  const VkImageMemoryBarrier barrier{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      /*pNext=*/nullptr,
      /*srcAccessMask=*/prev_usage.GetAccessFlags(),
      /*dstAccessMask=*/next_usage.GetAccessFlags(),
      /*oldLayout=*/prev_usage.GetImageLayout(),
      /*newLayout=*/next_usage.GetImageLayout(),
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
      /*srcStageMask=*/prev_usage.GetPipelineStageFlags(),
      /*dstStageMask=*/next_usage.GetPipelineStageFlags(),
      /*dependencyFlags=*/0,
      /*memoryBarrierCount=*/0,
      /*pMemoryBarriers=*/nullptr,
      /*bufferMemoryBarrierCount=*/0,
      /*pBufferMemoryBarriers=*/nullptr,
      /*imageMemoryBarrierCount=*/1,
      &barrier);
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
