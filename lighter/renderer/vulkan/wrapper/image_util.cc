//
//  image_util.cc
//
//  Created by Pujun Lun on 4/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/image_util.h"

#include <algorithm>
#include <queue>

#include "lighter/renderer/vulkan/wrapper/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

using UsageType = image::Usage::UsageType;
using AccessType = image::Usage::AccessType;
using AccessLocation = image::Usage::AccessLocation;

// A comparator used to build a priority queue of ImageAtStage objects, ordered
// by the stage in ascending order.
struct ImageAtStageComparator {
  bool operator()(const image::UsageAtStage& lhs,
                  const image::UsageAtStage& rhs) {
    return lhs.stage > rhs.stage;
  }
};

// Min heap of ImageAtStage objects ordered by the stage.
using ImageUsageAtStageQueue = std::priority_queue<
    image::UsageAtStage, std::vector<image::UsageAtStage>,
    ImageAtStageComparator>;

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

// Returns VkAccessFlags used for inserting image memory barriers.
VkAccessFlags GetAccessFlags(const image::Usage& usage) {
  switch (usage.usage_type) {
    case UsageType::kDontCare:
      return kNullAccessFlag;

    case UsageType::kRenderTarget:
      return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    case UsageType::kPresentation:
      FATAL("Should be handled by render pass");

    case UsageType::kLinearAccess:
    case UsageType::kSample:
      return usage.access_location == AccessLocation::kHost
                 ? GetReadWriteFlags(usage.access_type,
                                     VK_ACCESS_HOST_READ_BIT,
                                     VK_ACCESS_HOST_WRITE_BIT)
                 : GetReadWriteFlags(usage.access_type,
                                     VK_ACCESS_SHADER_READ_BIT,
                                     VK_ACCESS_SHADER_WRITE_BIT);

    case UsageType::kTransfer:
      return GetReadWriteFlags(usage.access_type,
                               VK_ACCESS_TRANSFER_READ_BIT,
                               VK_ACCESS_TRANSFER_WRITE_BIT);
  }
}

// Returns VkPipelineStageFlags used for inserting image memory barriers.
VkPipelineStageFlags GetPipelineStageFlags(const image::Usage& usage) {
  switch (usage.usage_type) {
    case UsageType::kDontCare:
      return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    case UsageType::kRenderTarget:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    case UsageType::kPresentation:
      FATAL("Should be handled by render pass");

    case UsageType::kLinearAccess:
    case UsageType::kSample:
      switch (usage.access_location) {
        case AccessLocation::kDontCare:
          FATAL("Access location not specified");
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

// Returns which image layout should be used for 'usage'.
VkImageLayout GetImageLayout(const image::Usage& usage) {
  switch (usage.usage_type) {
    case UsageType::kDontCare:
      return VK_IMAGE_LAYOUT_UNDEFINED;

    case UsageType::kRenderTarget:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    case UsageType::kPresentation:
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    case UsageType::kLinearAccess:
      return VK_IMAGE_LAYOUT_GENERAL;

    case UsageType::kSample:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    case UsageType::kTransfer:
      switch (usage.access_type) {
        case AccessType::kDontCare:
          FATAL("Access type not specified");
        case AccessType::kReadOnly:
          return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case AccessType::kWriteOnly:
          return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case AccessType::kReadWrite:
          FATAL("Access type must not be kReadWrite for UsageType::kTransfer");
      }
  }
}

// Returns VkImageUsageFlagBits corresponding to 'usage_type'. Note that this
// must not be called with UsageType::kDontCare since it doesn't have flag bits.
VkImageUsageFlagBits GetImageUsageFlagBits(const image::Usage& usage) {
  switch (usage.usage_type) {
    case UsageType::kDontCare:
      FATAL("No usage flag bits if don't care about usage");

    case UsageType::kRenderTarget:
    case UsageType::kPresentation:
      return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    case UsageType::kLinearAccess:
      return VK_IMAGE_USAGE_STORAGE_BIT;

    case UsageType::kSample:
      return VK_IMAGE_USAGE_SAMPLED_BIT;

    case UsageType::kTransfer:
      switch (usage.access_type) {
        case AccessType::kDontCare:
          FATAL("Access type not specified");
        case AccessType::kReadOnly:
          return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        case AccessType::kWriteOnly:
          return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        case AccessType::kReadWrite:
          FATAL("Access type must not be kReadWrite for UsageType::kTransfer");
      }
  }
}

} /* namespace */

namespace image {

void Usage::Validate() const {
  if (usage_type == UsageType::kLinearAccess) {
    ASSERT_FALSE(access_type == AccessType::kDontCare,
                 "Must specify access type for UsageType::kLinearAccess");
    ASSERT_FALSE(access_location == AccessLocation::kDontCare,
                 "Must specify access location for UsageType::kLinearAccess");
  }
  if (usage_type == UsageType::kSample) {
    ASSERT_FALSE(access_location == AccessLocation::kDontCare,
                 "Must specify access location for UsageType::kSample");
    ASSERT_FALSE(access_location == AccessLocation::kHost,
                 "Cannot use AccessLocation::kHost for UsageType::kSample");
  }
  if (usage_type == UsageType::kTransfer) {
    ASSERT_FALSE(access_type == AccessType::kDontCare,
                 "Must specify access type for UsageType::kTransfer");
    ASSERT_FALSE(access_type == AccessType::kReadWrite,
                 "Cannot use AccessType::kReadWrite for UsageType::kTransfer");
  }
}

std::vector<Usage> UsageInfo::GetAllUsages() const {
  std::vector<Usage> usages{initial_usage, final_usage};
  usages.reserve(usages.size() + usage_at_stages.size());
  for (const auto& usage_at_stage : usage_at_stages) {
    usages.push_back(usage_at_stage.usage);
  }
  return usages;
}

bool IsLinearAccessed(absl::Span<const Usage> usages) {
  return std::any_of(usages.begin(), usages.end(), [](const Usage& usage) {
    return usage.usage_type == UsageType::kLinearAccess;
  });
}

bool UseHighPrecision(absl::Span<const Usage> usages) {
  return std::any_of(usages.begin(), usages.end(), [](const Usage& usage) {
    return usage.use_high_precision;
  });
}

VkImageUsageFlags GetImageUsageFlags(absl::Span<const Usage> usages) {
  auto flags = nullflag;
  for (const auto& usage : usages) {
    if (usage.usage_type != UsageType::kDontCare) {
      flags |= GetImageUsageFlagBits(usage);
    }
  }
  return static_cast<VkImageUsageFlags>(flags);
}

LayoutManager::UsageHistory::UsageHistory(
    int num_stages, const image::UsageInfo& usage_info)
    : num_stages_{num_stages}, image_name_{usage_info.image_name} {
  // Validate usages.
  for (const auto& usage : usage_info.usage_at_stages) {
    usage.usage.Validate();
    ValidateStage(usage.stage);
  }

  // Put usages in a min heap, ordered by the stage. The initial usage is put at
  // stage -1 temporarily. We will shift all stages by 1 later.
  // If the user does not specify a final usage, we won't need to transition the
  // image layout, so we don't need to push it into the heap.
  ImageUsageAtStageQueue usage_queue{usage_info.usage_at_stages.begin(),
                                     usage_info.usage_at_stages.end()};
  usage_queue.push({usage_info.initial_usage, /*stage=*/-1});
  // TODO: We may need to insert memory barriers even if usages keep unchanged.
  if (usage_info.final_usage.usage_type != UsageType::kDontCare) {
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
                    absl::StrFormat("Conflicted image usages specified for %s "
                                    "at stage %d",
                                    image_name_, next_usage.stage));
        continue;
      }
    }
    usage_change_points_.push_back(next_usage);
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

void LayoutManager::UsageHistory::ValidateStage(int stage) const {
  ASSERT_TRUE(
      stage >= 0 && stage < num_stages_,
      absl::StrFormat("Stage must be in range [0, %d], while %d provided",
                      num_stages_ - 1, stage));
}

LayoutManager::LayoutManager(int num_stages, const UsageInfoMap& usage_info_map)
    : num_stages_{num_stages} {
  for (const auto& pair : usage_info_map) {
    image_usage_history_map_[pair.first] =
        absl::make_unique<UsageHistory>(num_stages_, pair.second);
  }
}

VkImageLayout LayoutManager::GetLayoutAtStage(const VkImage& image,
                                              int stage) const {
  const auto iter = image_usage_history_map_.find(&image);
  ASSERT_FALSE(iter == image_usage_history_map_.end(),
               "This manager does not have info about the image");
  return GetImageLayout(iter->second->GetUsageAtCurrentStage(stage));
}

void LayoutManager::InsertMemoryBarrierBeforeStage(
    const VkCommandBuffer& command_buffer,
    uint32_t queue_family_index, int stage) const {
  for (const auto& pair : image_usage_history_map_) {
    const auto& usage_history = pair.second;
    if (!usage_history->IsUsageChanged(stage)) {
      continue;
    }
    InsertMemoryBarrier(command_buffer, queue_family_index, *pair.first,
                        usage_history->GetUsageAtPreviousStage(stage),
                        usage_history->GetUsageAtCurrentStage(stage));
#ifndef NDEBUG
    LOG_INFO << absl::StreamFormat("Inserted memory barrier for image '%s' "
                                   "before stage %d",
                                   pair.second->image_name(), stage);
#endif /* !NDEBUG */
  }
}

void LayoutManager::InsertMemoryBarrierAfterFinalStage(
    const VkCommandBuffer& command_buffer, uint32_t queue_family_index) const {
  const int last_stage = num_stages_ - 1;
  for (const auto& pair : image_usage_history_map_) {
    const auto& usage_history = pair.second;
    if (!usage_history->IsUsageChangedAfterFinalStage()) {
      continue;
    }
    InsertMemoryBarrier(command_buffer, queue_family_index, *pair.first,
                        usage_history->GetUsageAtCurrentStage(last_stage),
                        usage_history->GetUsageAtNextStage(last_stage));
#ifndef NDEBUG
    LOG_INFO << absl::StreamFormat("Inserted memory barrier for image '%s' "
                                   "after final stage",
                                   pair.second->image_name());
#endif /* !NDEBUG */
  }
}

void LayoutManager::InsertMemoryBarrier(
    const VkCommandBuffer& command_buffer, uint32_t queue_family_index,
    const VkImage& image, image::Usage prev_usage,
    image::Usage curr_usage) const {
  const VkImageMemoryBarrier barrier{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      /*pNext=*/nullptr,
      /*srcAccessMask=*/GetAccessFlags(prev_usage),
      /*dstAccessMask=*/GetAccessFlags(curr_usage),
      /*oldLayout=*/GetImageLayout(prev_usage),
      /*newLayout=*/GetImageLayout(curr_usage),
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
      /*srcStageMask=*/GetPipelineStageFlags(prev_usage),
      /*dstStageMask=*/GetPipelineStageFlags(curr_usage),
      /*dependencyFlags=*/0,
      /*memoryBarrierCount=*/0,
      /*pMemoryBarriers=*/nullptr,
      /*bufferMemoryBarrierCount=*/0,
      /*pBufferMemoryBarriers=*/nullptr,
      /*imageMemoryBarrierCount=*/1,
      &barrier);
}

} /* namespace image */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
