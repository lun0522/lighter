//
//  base_pass.h
//
//  Created by Pujun Lun on 7/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_BASE_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_BASE_PASS_H

#include <string>

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/extension/image_util.h"
#include "lighter/renderer/vulkan/wrapper/image_usage.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/optional.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// The base class of compute pass and graphics pass.
class BasePass {
 public:
  // Maps image name to usage history.
  using ImageUsageHistoryMap = absl::flat_hash_map<std::string,
                                                   image::UsageHistory>;

  explicit BasePass(int num_subpasses) : num_subpasses_{num_subpasses} {}

  // This class is neither copyable nor movable.
  BasePass(const BasePass&) = delete;
  BasePass& operator=(const BasePass&) = delete;

  virtual ~BasePass() = default;

  // Returns the layout of image before this pass.
  VkImageLayout GetImageLayoutBeforePass(const std::string& image_name) const;

  // Returns the layout of image after this pass.
  VkImageLayout GetImageLayoutAfterPass(const std::string& image_name) const;

  // Returns the layout of image at 'subpass'. The usage at this subpass must
  // have been specified via AddUsage() in the usage history.
  VkImageLayout GetImageLayoutAtSubpass(const std::string& image_name,
                                        int subpass) const;

  // Updates the image usage tracked by 'usage_tracker' to the last usage of
  // that image in this pass.
  void UpdateTrackedImageUsage(const std::string& image_name,
                               image::UsageTracker& usage_tracker) const;

 protected:
  // Holds the previous and current image usage.
  struct ImageUsagesInfo {
    ImageUsagesInfo(int prev_usage_subpass,
                    const image::Usage* prev_usage,
                    const image::Usage* curr_usage)
        : prev_usage_subpass{prev_usage_subpass},
          prev_usage{*FATAL_IF_NULL(prev_usage)},
          curr_usage{*FATAL_IF_NULL(curr_usage)} {}

    const int prev_usage_subpass;
    const image::Usage& prev_usage;
    const image::Usage& curr_usage;
  };

  // Adds an image that is used in this pass. This checks whether subpasses
  // stored in the history are out of range.
  void AddUsageHistory(std::string&& image_name, image::UsageHistory&& history);

  // Returns the usage history of image.
  const image::UsageHistory& GetUsageHistory(
      const std::string& image_name) const;

  // Returns a pointer to image usage at 'subpass', or nullptr if the usage has
  // not been specified for that subpass.
  const image::Usage* GetImageUsage(const std::string& image_name,
                                    int subpass) const;

  // Returns previous and current image usages info if the image is used at
  // 'subpass' and synchronization on image memory access is needed,
  absl::optional<ImageUsagesInfo> GetImageUsagesIfNeedSynchronization(
      const std::string& image_name, int subpass) const;

  // Checks whether 'subpass' is in range:
  //   - [0, 'num_subpasses_'), if 'include_virtual_subpasses' is false.
  //   - [virtual_initial_subpass_index(), virtual_final_subpass_index()], if
  //     'include_virtual_subpasses' is true.
  void ValidateSubpass(int subpass, const std::string& image_name,
                       bool include_virtual_subpasses) const;

  // Images are in their initial/final layouts at the virtual subpasses.
  int virtual_initial_subpass_index() const { return -1; }
  int virtual_final_subpass_index() const { return num_subpasses_; }

  // Accessors.
  const ImageUsageHistoryMap& image_usage_history_map() const {
    return image_usage_history_map_;
  }

  // Number of subpasses.
  const int num_subpasses_;

 private:
  // Maps images used in this pass to their respective usage history.
  ImageUsageHistoryMap image_usage_history_map_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_BASE_PASS_H */
