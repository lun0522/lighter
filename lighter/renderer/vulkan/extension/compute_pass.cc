//
//  compute_pass.cc
//
//  Created by Pujun Lun on 6/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/compute_pass.h"

#include <iterator>

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {

ImageComputeUsageHistory& ImageComputeUsageHistory::AddUsage(
    int stage, const image::Usage& usage) {
  ASSERT_TRUE(
      usage_at_stage_map_.find(stage) == usage_at_stage_map_.end(),
      absl::StrFormat("Already specified usage for image %s at stage %d",
                      image_name_, stage));
  usage_at_stage_map_.insert({stage, usage});
  return *this;
}

ImageComputeUsageHistory& ImageComputeUsageHistory::SetFinalUsage(
    const image::Usage& usage) {
  ASSERT_NO_VALUE(final_usage_,
                  absl::StrFormat("Already specified final usage for image %s",
                                  image_name_));
  final_usage_ = usage;
  return *this;
}

std::vector<image::Usage> ImageComputeUsageHistory::GetAllUsages() const {
  const int num_usages = static_cast<int>(usage_at_stage_map_.size()) +
                         (final_usage_.has_value() ? 1 : 0);
  std::vector<image::Usage> usages;
  usages.reserve(num_usages);
  for (const auto& pair : usage_at_stage_map_) {
    usages.push_back(pair.second);
  }
  if (final_usage_.has_value()) {
    usages.push_back(final_usage_.value());
  }
  return usages;
}

void ImageComputeUsageHistory::ValidateStages(int upper_bound) const {
  for (const auto& pair : usage_at_stage_map_) {
    const int stage = pair.first;
    ASSERT_TRUE(
        stage >= 0 && stage < upper_bound,
        absl::StrFormat("Stage (%d) out of range [0, %d) for image %s",
                        stage, upper_bound, image_name_));
  }
}

const image::Usage& ImageComputeUsageHistory::GetUsage(int stage) const {
  const auto iter = usage_at_stage_map_.find(stage);
  ASSERT_FALSE(iter == usage_at_stage_map_.end(),
               absl::StrFormat("Usage not specified for image %s",
                               image_name_));
  return iter->second;
}

ComputePass& ComputePass::AddImage(Image* image,
                                   ImageComputeUsageHistory&& history) {
  ASSERT_NON_NULL(image, "'image' cannot be nullptr");
  history.ValidateStages(num_stages_);
  history.AddUsage(/*stage=*/-1, image->image_usage());
  image_usage_history_map_.emplace(image, std::move(history));
  return *this;
}

VkImageLayout ComputePass::GetImageLayoutAtStage(const Image& image,
                                                 int stage) const {
  const auto iter = image_usage_history_map_.find(&image);
  ASSERT_FALSE(iter == image_usage_history_map_.end(), "Unrecognized image");
  return iter->second.GetUsage(stage).GetImageLayout();
}

void ComputePass::Run(const VkCommandBuffer& command_buffer,
                      uint32_t queue_family_index,
                      absl::Span<const ComputeOp> compute_ops) const {
  ASSERT_TRUE(compute_ops.size() == num_stages_,
              absl::StrFormat("Size of 'compute_ops' (%d) mismatches with the "
                              "number of stages (%d)",
                              compute_ops.size(), num_stages_));

  // Run all stages and insert memory barriers. Note that even if the image
  // usage does not change, we still need to insert a memory barrier if not RAR.
  for (int stage = 0; stage < num_stages_; ++stage) {
    for (const auto& pair : image_usage_history_map_) {
      const auto& usage_at_stage_map = pair.second.usage_at_stage_map_;
      const auto iter = usage_at_stage_map.find(stage);
      if (iter == usage_at_stage_map.end()) {
        continue;
      }

      const image::Usage& next_usage = iter->second;
      const image::Usage& prev_usage = std::prev(iter)->second;
      if (next_usage == prev_usage &&
          next_usage.access_type == image::Usage::AccessType::kReadOnly) {
        continue;
      }

      InsertMemoryBarrier(command_buffer, queue_family_index, **pair.first,
                          prev_usage, next_usage);
#ifndef NDEBUG
      LOG_INFO << absl::StreamFormat("Inserted memory barrier for image '%s' "
                                     "before stage %d",
                                     pair.second.image_name_, stage);
#endif /* !NDEBUG */
    }
    compute_ops[stage]();
  }

  // For each image, inform Image class the last known usage, and transition the
  // layout if necessary. Note that if the final usage is the same as the
  // previous usage, there is no need to insert a memory barrier.
  for (auto& pair : image_usage_history_map_) {
    Image& image = *pair.first;
    const ImageComputeUsageHistory& history = pair.second;
    const image::Usage& prev_usage =
        history.usage_at_stage_map_.rbegin()->second;
    if (!history.final_usage_.has_value()) {
      image.set_usage(prev_usage);
      continue;
    }

    const image::Usage& final_usage = history.final_usage_.value();
    image.set_usage(final_usage);
    if (final_usage == prev_usage) {
      continue;
    }

    InsertMemoryBarrier(command_buffer, queue_family_index, **pair.first,
                        prev_usage, final_usage);
#ifndef NDEBUG
    LOG_INFO << absl::StreamFormat("Inserted memory barrier for image '%s' "
                                   "after final stage",
                                   pair.second.image_name_);
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
