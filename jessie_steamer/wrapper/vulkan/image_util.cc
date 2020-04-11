//
//  image_util.cc
//
//  Created by Pujun Lun on 4/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/image_util.h"

#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/absl/strings/str_format.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

VkImageLayout GetImageLayout(ImageLayoutManager::ImageUsage usage) {
  using ImageUsage = ImageLayoutManager::ImageUsage;
  switch (usage) {
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

ImageLayoutManager::ImageLayoutManager(int num_stages,
                                       absl::Span<const UsageInfo> infos)
    : num_stages_{num_stages} {
  std::for_each(infos.begin(), infos.end(),
                [this](const UsageInfo& info) { BuildUsageMap(info); });
}

void ImageLayoutManager::ValidateStage(int stage) const {
  ASSERT_TRUE(
      stage >= 0 && stage < num_stages_,
      absl::StrFormat("Stage must be in range [0, %d], while %d provided",
                      num_stages_ - 1, stage));
}

void ImageLayoutManager::BuildUsageMap(const UsageInfo& info) {
  ASSERT_TRUE(image_layouts_map_.find(info.image) == image_layouts_map_.end(),
              absl::StrFormat("Duplicated image usages specified for %s",
                              info.image_name));

  absl::flat_hash_map<int, ImageUsage> usage_map;
  for (const auto& usage_at_stage : info.usages) {
    ValidateStage(usage_at_stage.stage);
    const auto iter = usage_map.find(usage_at_stage.stage);
    if (iter != usage_map.end()) {
      ASSERT_TRUE(iter->second == usage_at_stage.usage,
                  absl::StrFormat("Conflicted image usages specified for %s: "
                                  "%d vs %d at stage %d",
                                  info.image_name,
                                  static_cast<int>(iter->second),
                                  static_cast<int>(usage_at_stage.usage),
                                  usage_at_stage.stage));
    }
    usage_map[usage_at_stage.stage] = usage_at_stage.usage;
  }

  // Assign to stages where the image layout should be changed.
  auto& layouts = image_layouts_map_[info.image];
  layouts.resize(num_stages_ + 1, VK_IMAGE_LAYOUT_UNDEFINED);
  layouts[0] = info.initial_layout;
  for (const auto& pair : usage_map) {
    layouts[pair.first + 1] = GetImageLayout(pair.second);
  }

  // Assign to stages where the layout does not change from the previous stage.
  VkImageLayout previous_layout = layouts[0];
  for (int i = 1; i < layouts.size(); ++i) {
    if (layouts[i] == VK_IMAGE_LAYOUT_UNDEFINED) {
      layouts[i] = previous_layout;
    } else {
      previous_layout = layouts[i];
    }
  }
}

VkImageLayout ImageLayoutManager::GetImageLayoutAtStage(const VkImage& image,
                                                        int stage) const {
  const auto iter = image_layouts_map_.find(&image);
  ASSERT_FALSE(iter == image_layouts_map_.end(),
               "This manager does not have info about the image");
  ValidateStage(stage);
  return iter->second[stage + 1];
}

bool ImageLayoutManager::NeedMemoryBarrierBeforeStage(int stage) const {
  ValidateStage(stage);
  for (const auto& pair : image_layouts_map_) {
    const auto& layouts = pair.second;
    if (layouts[stage] != layouts[stage + 1]) {
      return true;
    }
  }
  return false;
}

void ImageLayoutManager::InsertMemoryBarrierBeforeStage(
    const VkCommandBuffer& command_buffer,
    uint32_t queue_family_index, int stage) const {
  ValidateStage(stage);

  std::vector<VkImageMemoryBarrier> barriers;
  for (const auto& pair : image_layouts_map_) {
    const auto& layouts = pair.second;
    if (layouts[stage] != layouts[stage + 1]) {
      barriers.emplace_back(VkImageMemoryBarrier{
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          /*pNext=*/nullptr,
          /*srcAccessMask=*/kNullAccessFlag,
          /*dstAccessMask=*/kNullAccessFlag,
          layouts[stage],
          layouts[stage + 1],
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
