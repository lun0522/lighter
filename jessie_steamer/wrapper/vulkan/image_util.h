//
//  image_util.h
//
//  Created by Pujun Lun on 4/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_UTIL_H

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/common/util.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class ImageLayoutManager {
 public:
  enum class ImageUsage {
    kDontCare,
    kRenderingTarget,
    kSampledAsTexture,
    kPresentToScreen,
    kSrcOfCopyOnDevice,
    kDstOfCopyOnDevice,
    kLinearReadByShader,
    kLinearWriteByShader,
    kLinearReadByHost,
    kLinearWriteByHost,
  };

  struct UsageAtStage {
    ImageUsage usage;
    int stage;
  };

  struct UsageInfo {
    UsageInfo(const VkImage* image, std::string&& image_name)
        : image{FATAL_IF_NULL(image)}, image_name{std::move(image_name)} {}

    UsageInfo& SetInitialUsage(ImageUsage usage) {
      initial_usage = usage;
      return *this;
    }

    UsageInfo& SetFinalUsage(ImageUsage usage) {
      final_usage = usage;
      return *this;
    }

    UsageInfo& AddUsage(int stage, ImageUsage usage) {
      usages.emplace_back(UsageAtStage{usage, stage});
      return *this;
    }

    const VkImage* image;
    const std::string image_name;
    ImageUsage initial_usage = ImageUsage::kDontCare;
    ImageUsage final_usage = ImageUsage::kDontCare;
    std::vector<UsageAtStage> usages;
  };

  ImageLayoutManager(int num_stages, absl::Span<const UsageInfo> usage_infos);

  // This class is neither copyable nor movable.
  ImageLayoutManager(const ImageLayoutManager&) = delete;
  ImageLayoutManager& operator=(const ImageLayoutManager&) = delete;

  VkImageLayout GetImageLayoutAtStage(const VkImage& image, int stage) const;

  bool NeedMemoryBarrierBeforeStage(int stage) const;

  void InsertMemoryBarrierBeforeStage(const VkCommandBuffer& command_buffer,
                                      uint32_t queue_family_index,
                                      int stage) const;

 private:
  class ImageUsageHistory {
   public:
    ImageUsageHistory(int num_stages, const UsageInfo& usage_info);

    // This class is neither copyable nor movable.
    ImageUsageHistory(const ImageUsageHistory&) = delete;
    ImageUsageHistory& operator=(const ImageUsageHistory&) = delete;

    bool IsUsageChanged(int stage) const {
      ValidateStage(stage);
      return usage_at_stages_[stage] != usage_at_stages_[stage + 1];
    }

    ImageUsage GetUsageAtStage(int stage) const {
      ValidateStage(stage);
      return usage_at_stages_[stage + 1]->usage;
    }

    ImageUsage GetUsageAtPreviousStage(int stage) const {
      ValidateStage(stage);
      return std::prev(usage_at_stages_[stage + 1])->usage;
    }

    ImageUsage GetUsageAtNextStage(int stage) const {
      ValidateStage(stage);
      return std::next(usage_at_stages_[stage + 1])->usage;
    }

   private:
    void ValidateStage(int stage) const;

    const int num_stages_;
    std::vector<UsageAtStage> usage_change_points_;
    std::vector<std::vector<UsageAtStage>::iterator> usage_at_stages_;
  };

  absl::flat_hash_map<const VkImage*, std::unique_ptr<ImageUsageHistory>>
      image_usage_history_map_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_UTIL_H */
