//
//  image_util.cc
//
//  Created by Pujun Lun on 4/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/image_util.h"

#include <queue>

#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using ImageUsageAtStage = ImageLayoutManager::UsageAtStage;

struct ImageAtStageComparator {
  bool operator()(const ImageUsageAtStage& lhs, const ImageUsageAtStage& rhs) {
    return lhs.stage > rhs.stage;
  }
};

using ImageUsageAtStageQueue = std::priority_queue<
    ImageUsageAtStage, std::vector<ImageUsageAtStage>, ImageAtStageComparator>;

VkImageLayout GetImageLayout(ImageLayoutManager::ImageUsage usage) {
  using ImageUsage = ImageLayoutManager::ImageUsage;
  switch (usage) {
    case ImageUsage::kDontCare:
      return VK_IMAGE_LAYOUT_UNDEFINED;
    case ImageUsage::kRenderingTarget:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ImageUsage::kSampledAsTexture:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ImageUsage::kPresentToScreen:
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case ImageUsage::kSrcOfCopyOnDevice:
      return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ImageUsage::kDstOfCopyOnDevice:
      return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ImageUsage::kLinearReadByShader:
    case ImageUsage::kLinearWriteByShader:
    case ImageUsage::kLinearReadByHost:
    case ImageUsage::kLinearWriteByHost:
      return VK_IMAGE_LAYOUT_GENERAL;
  }
}

} /* namespace */

ImageLayoutManager::ImageUsageHistory::ImageUsageHistory(
    int num_stages, const UsageInfo& usage_info)
    : num_stages_{num_stages} {
  for (const auto& usage : usage_info.usages) {
    ValidateStage(usage.stage);
  }

  // Put usages in a min heap, ordered by the stage. The initial usage is put at
  // stage -1 temporarily. We will shift all stages by 1 later.
  // If the user does not specify a final usage, we won't need to transition the
  // image layout, so we don't need to push it into the heap.
  ImageUsageAtStageQueue usage_queue{usage_info.usages.begin(),
                                     usage_info.usages.end()};
  usage_queue.push({usage_info.initial_usage, /*stage=*/-1});
  if (usage_info.final_usage != ImageUsage::kDontCare) {
    usage_queue.push({usage_info.final_usage, /*stage=*/num_stages_});
  }

  // Pop usages out of the heap and populate 'usage_change_points_'. If the next
  // usage is the same to the previous one, we won't need to transition the
  // image layout, so we don't need to store it anymore. Note that we don't
  // allow the user to specify different usages for one stage.
  while (!usage_queue.empty()) {
    const auto next_usage = usage_queue.top();
    usage_queue.pop();
    if (!usage_change_points_.empty()) {
      const auto& last_change_point = usage_change_points_.back();
      if (next_usage.usage == last_change_point.usage) {
        continue;
      }
      if (next_usage.stage == last_change_point.stage) {
        ASSERT_TRUE(next_usage.usage == last_change_point.usage,
                    absl::StrFormat("Conflicted image usages specified for %s: "
                                    "%d vs %d at stage %d",
                                    usage_info.image_name,
                                    static_cast<int>(next_usage.usage),
                                    static_cast<int>(last_change_point.usage),
                                    next_usage.stage));
        continue;
      }
    }
    usage_change_points_.emplace_back(next_usage);
  }

  // Shift stages by 1, so that all stages are non-negative, and the usage at
  // index 0 represents the initial usage.
  for (auto& change_point : usage_change_points_) {
    ++change_point.stage;
  }
  ASSERT_TRUE(usage_change_points_[0].usage == usage_info.initial_usage &&
                  usage_change_points_[0].stage == 0,
              "The first change point must be the initial usage");

  // Populate 'usage_at_stages_'. Each element should be an iterator referencing
  // to the usage change point that defines the usage at the current stage.
  usage_at_stages_.resize(num_stages_ + 2, usage_change_points_.end());
  for (auto iter = usage_change_points_.begin();
       iter != usage_change_points_.end(); ++iter) {
    usage_at_stages_[iter->stage] = iter;
  }
  auto change_point_iter = usage_change_points_.end();
  for (auto& usage : usage_at_stages_) {
    if (usage == usage_change_points_.end()) {
      usage = change_point_iter;
    } else {
      change_point_iter = usage;
    }
  }
}

void ImageLayoutManager::ImageUsageHistory::ValidateStage(int stage) const {
  ASSERT_TRUE(
      stage >= 0 && stage < num_stages_,
      absl::StrFormat("Stage must be in range [0, %d], while %d provided",
                      num_stages_ - 1, stage));
}

ImageLayoutManager::ImageLayoutManager(
    int num_stages, absl::Span<const UsageInfo> usage_infos) {
  for (const auto& info : usage_infos) {
    ASSERT_TRUE(image_usage_history_map_.find(info.image) ==
                image_usage_history_map_.end(),
                absl::StrFormat("Duplicated image usages specified for %s",
                                info.image_name));
    image_usage_history_map_[info.image] =
        absl::make_unique<ImageUsageHistory>(num_stages, info);
  }
}

VkImageLayout ImageLayoutManager::GetImageLayoutAtStage(const VkImage& image,
                                                        int stage) const {
  const auto iter = image_usage_history_map_.find(&image);
  ASSERT_FALSE(iter == image_usage_history_map_.end(),
               "This manager does not have info about the image");
  return GetImageLayout(iter->second->GetUsageAtStage(stage));
}

bool ImageLayoutManager::NeedMemoryBarrierBeforeStage(int stage) const {
  for (const auto& pair : image_usage_history_map_) {
    if (pair.second->IsUsageChanged(stage)) {
      return true;
    }
  }
  return false;
}

void ImageLayoutManager::InsertMemoryBarrierBeforeStage(
    const VkCommandBuffer& command_buffer,
    uint32_t queue_family_index, int stage) const {
  std::vector<VkImageMemoryBarrier> barriers;
  for (const auto& pair : image_usage_history_map_) {
    const auto& usage_history = pair.second;
    if (usage_history->IsUsageChanged(stage)) {
      barriers.emplace_back(VkImageMemoryBarrier{
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          /*pNext=*/nullptr,
          /*srcAccessMask=*/kNullAccessFlag,
          /*dstAccessMask=*/kNullAccessFlag,
          GetImageLayout(usage_history->GetUsageAtPreviousStage(stage)),
          GetImageLayout(usage_history->GetUsageAtStage(stage)),
          /*srcQueueFamilyIndex=*/queue_family_index,
          /*dstQueueFamilyIndex=*/queue_family_index,
          *pair.first,
          VkImageSubresourceRange{
              VK_IMAGE_ASPECT_COLOR_BIT,
              /*baseMipLevel=*/0,
              /*levelCount=*/1,
              /*baseArrayLayer=*/0,
              /*layerCount=*/1,
          },
      });
    }
  }

  if (!barriers.empty()) {
#ifndef NDEBUG
    LOG_INFO << absl::StreamFormat("Inserted %d barriers before stage %d",
                                   barriers.size(), stage);
#endif /* !NDEBUG */

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        /*dependencyFlags=*/0,
        /*memoryBarrierCount=*/0,
        /*pMemoryBarriers=*/nullptr,
        /*bufferMemoryBarrierCount=*/0,
        /*pBufferMemoryBarriers=*/nullptr,
        CONTAINER_SIZE(barriers),
        barriers.data());
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
