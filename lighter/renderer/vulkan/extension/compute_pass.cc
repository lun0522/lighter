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
#include "lighter/renderer/vulkan/wrapper/image_usage.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {

ComputePass& ComputePass::AddImage(Image* image,
                                   image::UsageHistory&& history) {
  BasePass::AddImageAndHistory(image, std::move(history));
  return *this;
}

void ComputePass::Run(const VkCommandBuffer& command_buffer,
                      uint32_t queue_family_index,
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
      const auto usages_info =
          GetImageUsagesIfNeedSynchronization(/*history=*/pair.second, subpass);
      if (!usages_info.has_value()) {
        continue;
      }

      InsertMemoryBarrier(command_buffer, queue_family_index, **pair.first,
                          usages_info.value().prev_usage,
                          usages_info.value().curr_usage);
#ifndef NDEBUG
      const std::string log_suffix =
          subpass == virtual_final_subpass_index()
              ? "after compute pass"
              : absl::StrFormat("before subpass %d", subpass);
      LOG_INFO << absl::StreamFormat("Inserted memory barrier for image '%s' ",
                                     pair.second.image_name())
               << log_suffix;
#endif /* !NDEBUG */
    }

    if (subpass < num_subpasses_) {
      compute_ops[subpass]();
    }
  }

  // Set the final usage through Image class for each image.
  for (const auto& pair : image_usage_history_map()) {
    pair.first->set_usage(pair.second.usage_at_subpass_map().rbegin()->second);
  }
}

void ComputePass::InsertMemoryBarrier(
    const VkCommandBuffer& command_buffer, uint32_t queue_family_index,
    const VkImage& image, const image::Usage& prev_usage,
    const image::Usage& curr_usage) const {
  const VkImageMemoryBarrier barrier{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      /*pNext=*/nullptr,
      /*srcAccessMask=*/prev_usage.GetAccessFlags(),
      /*dstAccessMask=*/curr_usage.GetAccessFlags(),
      /*oldLayout=*/prev_usage.GetImageLayout(),
      /*newLayout=*/curr_usage.GetImageLayout(),
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
      /*dstStageMask=*/curr_usage.GetPipelineStageFlags(),
      /*dependencyFlags=*/0,
      /*memoryBarrierCount=*/0,
      /*pMemoryBarriers=*/nullptr,
      /*bufferMemoryBarrierCount=*/0,
      /*pBufferMemoryBarriers=*/nullptr,
      /*imageMemoryBarrierCount=*/1,
      &barrier);
}

void ComputePass::ValidateImageUsageHistory(
    const image::UsageHistory& history) const {
  using ImageUsageType = image::Usage::UsageType;
  for (const auto& pair : history.usage_at_subpass_map()) {
    const ImageUsageType usage_type = pair.second.usage_type();
    ASSERT_TRUE(usage_type == ImageUsageType::kLinearAccess
                    || usage_type == ImageUsageType::kSample
                    || usage_type == ImageUsageType::kTransfer,
                absl::StrFormat("Usage type (%d) is neither kLinearAccess, "
                                "kSample nor kTransfer for image '%s' at "
                                "subpass %d",
                                usage_type, history.image_name(), pair.first));
  }
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
