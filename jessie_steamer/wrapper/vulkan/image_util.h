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
namespace image {

// Usages of images that we can handle.
// TODO: Break down to read/write + usage.
enum class Usage {
  kDontCare,
  kRenderingTarget,
  kPresentToScreen,
  kSrcOfCopyOnDevice,
  kDstOfCopyOnDevice,
  kSampledInFragmentShader,
  kSampledInComputeShader,
  kLinearReadInFragmentShader,
  kLinearReadInComputeShader,
  kLinearWriteInFragmentShader,
  kLinearWriteInComputeShader,
  kLinearReadWriteInFragmentShader,
  kLinearReadWriteInComputeShader,
  kLinearReadByHost,
  kLinearWriteByHost,
};

// Each image can have only one usage at one stage.
struct UsageAtStage {
  Usage usage;
  int stage;
};

// Holds usages of one image at all stages. If the usage is not specified for a
// certain stage, we assume that either the image is not used in that stage, or
// the usage remains the same to the previous stage.
// Initial usage and final usage are usages prior to or after these stages.
struct UsageInfo {
  explicit UsageInfo(std::string&& image_name)
      : image_name{std::move(image_name)} {}

  // Returns all usages at all stages, including initial and final usages.
  // Note that this can contain duplicates.
  std::vector<Usage> GetAllUsages() const;

  // Modifiers.
  UsageInfo& SetInitialUsage(Usage usage) {
    initial_usage = usage;
    return *this;
  }
  UsageInfo& SetFinalUsage(Usage usage) {
    final_usage = usage;
    return *this;
  }
  UsageInfo& AddUsage(int stage, Usage usage) {
    usage_at_stages.emplace_back(UsageAtStage{usage, stage});
    return *this;
  }

  const std::string image_name;
  Usage initial_usage = Usage::kDontCare;
  Usage final_usage = Usage::kDontCare;
  std::vector<UsageAtStage> usage_at_stages;
};

// Returns a VkImageUsageFlags that contains all usages. Note that if all usages
// are Usage::kDontCare, an error will be thrown since there will be no
// corresponding flags.
VkImageUsageFlags GetImageUsageFlags(absl::Span<const Usage> usages);
inline VkImageUsageFlags GetImageUsageFlags(const UsageInfo& usage_info) {
  return GetImageUsageFlags(usage_info.GetAllUsages());
}

// This class is used for tracking usages of images, and inserting memory
// barriers for transitioning image layouts when necessary.
// TODO: We should extend this to build high-level description of image usage,
// and also build render pass with this.
class LayoutManager {
 public:
  // Maps each image to the corresponding usage info.
  using UsageInfoMap = absl::flat_hash_map<const VkImage*, image::UsageInfo>;

  LayoutManager(int num_stages, const UsageInfoMap& usage_info_map);

  // This class is neither copyable nor movable.
  LayoutManager(const LayoutManager&) = delete;
  LayoutManager& operator=(const LayoutManager&) = delete;

  // Returns the layout of 'image' at 'stage'.
  VkImageLayout GetLayoutAtStage(const VkImage& image, int stage) const;

  // Returns whether we need to insert memory barriers before 'stage' for
  // transitioning image layouts.
  bool NeedMemoryBarrierBeforeStage(int stage) const;

  // Inserts memory barriers before 'stage' for transitioning image layouts,
  // using the queue with 'queue_family_index'.
  // This should be called when 'command_buffer' is recording commands.
  void InsertMemoryBarrierBeforeStage(const VkCommandBuffer& command_buffer,
                                      uint32_t queue_family_index,
                                      int stage) const;

  // Inserts memory barriers for transitioning images to their final layouts
  // using the queue with 'queue_family_index'.
  // This should be called when 'command_buffer' is recording commands.
  void InsertMemoryBarrierAfterFinalStage(const VkCommandBuffer& command_buffer,
                                          uint32_t queue_family_index) const;

 private:
  // This class analyzes the given UsageInfo of an image, and builds a usage
  // history with which we can query the usage in previous/current/next stage
  // of any specific stage.
  class UsageHistory {
   public:
    UsageHistory(int num_stages, const image::UsageInfo& usage_info);

    // This class is neither copyable nor movable.
    UsageHistory(const UsageHistory&) = delete;
    UsageHistory& operator=(const UsageHistory&) = delete;

    // Returns whether the usage of image is changed at the beginning of
    // 'stage'.
    bool IsUsageChanged(int stage) const {
      ValidateStage(stage);
      return usage_at_stages_[stage] != usage_at_stages_[stage + 1];
    }

    // Returns whether the usage of image is changed after the final stage.
    bool IsUsageChangedAfterFinalStage() const {
      return usage_at_stages_[num_stages_] != usage_at_stages_[num_stages_ + 1];
    }

    // Returns the image usage at the previous/current/next stage.
    image::Usage GetUsageAtPreviousStage(int current_stage) const {
      ValidateStage(current_stage);
      return std::prev(usage_at_stages_[current_stage + 1])->usage;
    }
    image::Usage GetUsageAtCurrentStage(int current_stage) const {
      ValidateStage(current_stage);
      return usage_at_stages_[current_stage + 1]->usage;
    }
    image::Usage GetUsageAtNextStage(int current_stage) const {
      ValidateStage(current_stage);
      return std::next(usage_at_stages_[current_stage + 1])->usage;
    }

    // Accessors.
    const std::string& image_name() const { return image_name_; }

   private:
    // Validates that 'stage' is within range [0, 'num_stages_' - 1].
    void ValidateStage(int stage) const;

    // Number of stages.
    const int num_stages_;

    // Name of image, only used for debugging.
    const std::string image_name_;

    // Stores the stage where the image usage changes, which means before such a
    // stage, we have to use a memory barrier to transition the image layout.
    std::vector<image::UsageAtStage> usage_change_points_;

    // Elements are indexed by stage (initial and final usages are at index 0
    // and 'num_stages_' + 1). Each element references to one element in
    // 'usage_change_points_' which defines the layout at this stage.
    // This enables us to use 'usage_change_points_' like a doubly linked list,
    // so that we can quickly look up the usage at the current stage, and also
    // the previous/next usage, to determine access masks, pipeline stages, etc.
    // for inserting memory barriers.
    std::vector<std::vector<image::UsageAtStage>::iterator> usage_at_stages_;
  };

  // Inserts a memory barrier for transitioning the layout of 'image' using the
  // queue with 'queue_family_index', so that it can be used for a different
  // purpose. This should be called when 'command_buffer' is recording commands.
  void InsertMemoryBarrier(const VkCommandBuffer& command_buffer,
                           uint32_t queue_family_index, const VkImage& image,
                           image::Usage prev_usage,
                           image::Usage curr_usage) const;

  // Number of stages.
  const int num_stages_;

  // Maps each image to the corresponding usage history.
  absl::flat_hash_map<const VkImage*, std::unique_ptr<UsageHistory>>
      image_usage_history_map_;
};

} /* namespace image */
} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_UTIL_H */
