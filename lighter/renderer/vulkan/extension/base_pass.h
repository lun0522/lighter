//
//  base_pass.h
//
//  Created by Pujun Lun on 7/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_BASE_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_BASE_PASS_H

#include <vulkan/vulkan.h>

#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/ir/image_usage.h"

namespace lighter {
namespace renderer {
namespace vulkan {

using ir::ImageUsage;

// Holds usages of an image across subpasses of a legacy Gen 1 pass.
class ImageUsageHistory {
 public:
  using MultisampleResolveSourceMap = std::unordered_map<int, std::string>;

  explicit ImageUsageHistory(const ImageUsage& initial_usage)
      : initial_usage_{initial_usage} {}
  explicit ImageUsageHistory() = default;

  ImageUsageHistory(ImageUsageHistory&&) noexcept = default;
  ImageUsageHistory& operator=(ImageUsageHistory&&) noexcept = default;

  ImageUsageHistory& AddUsage(int subpass, const ImageUsage& usage) {
    const auto did_insert =
        usage_at_subpass_map_.insert({subpass, usage}).second;
    ASSERT_TRUE(did_insert,
                std::format("Already specified usage for subpass {}", subpass));
    return *this;
  }

  ImageUsageHistory& AddUsage(int subpass_start, int subpass_end,
                              const ImageUsage& usage) {
    ASSERT_TRUE(
        subpass_start <= subpass_end,
        std::format("Invalid range [{}, {}]", subpass_start, subpass_end));
    for (int subpass = subpass_start; subpass <= subpass_end; ++subpass) {
      AddUsage(subpass, usage);
    }
    return *this;
  }

  ImageUsageHistory& AddMultisampleResolveSource(
      int subpass, std::string_view source_image_name) {
    const auto iter =
        resolve_source_map_.insert({subpass, std::string{source_image_name}});
    ASSERT_TRUE(
        iter.second,
        std::format("Already specified source for subpass {}", subpass));
    return AddUsage(subpass, ImageUsage::GetMultisampleResolveTargetUsage());
  }

  ImageUsageHistory& SetFinalUsage(const ImageUsage& usage) {
    ASSERT_NO_VALUE(final_usage_, "Already specified final usage");
    final_usage_ = usage;
    return *this;
  }

  std::vector<ImageUsage> GetAllUsages() const {
    const int num_usages = 1 + static_cast<int>(usage_at_subpass_map_.size()) +
                           (final_usage_.has_value() ? 1 : 0);
    std::vector<ImageUsage> usages;
    usages.reserve(num_usages);
    usages.push_back(initial_usage_);
    for (const auto& pair : usage_at_subpass_map_) {
      usages.push_back(pair.second);
    }
    if (final_usage_.has_value()) {
      usages.push_back(final_usage_.value());
    }
    return usages;
  }

  const std::map<int, ImageUsage>& usage_at_subpass_map() const {
    return usage_at_subpass_map_;
  }
  const ImageUsage& initial_usage() const { return initial_usage_; }
  const std::optional<ImageUsage>& final_usage() const { return final_usage_; }
  const MultisampleResolveSourceMap& multisample_resolve_source_map() const {
    return resolve_source_map_;
  }

 private:
  std::map<int, ImageUsage> usage_at_subpass_map_;
  ImageUsage initial_usage_;
  std::optional<ImageUsage> final_usage_;
  MultisampleResolveSourceMap resolve_source_map_;
};

// Tracks legacy Gen 1 usages across multiple named images.
class ImageUsageTracker {
 public:
  ImageUsageTracker() = default;

  ImageUsageTracker(const ImageUsageTracker&) = delete;
  ImageUsageTracker& operator=(const ImageUsageTracker&) = delete;

  ImageUsageTracker& TrackImage(std::string&& image_name,
                                const ImageUsage& current_usage) {
    ASSERT_FALSE(
        image_usage_map_.contains(image_name),
        std::format("Already tracking image with name '{}'", image_name));
    image_usage_map_.insert({std::move(image_name), current_usage});
    return *this;
  }

  ImageUsageTracker& TrackImage(const std::string& image_name,
                                const ImageUsage& current_usage) {
    return TrackImage(std::string{image_name}, current_usage);
  }

  bool IsImageTracked(const std::string& image_name) const {
    return image_usage_map_.contains(image_name);
  }

  const ImageUsage& GetUsage(const std::string& image_name) const {
    const auto iter = image_usage_map_.find(image_name);
    ASSERT_FALSE(iter == image_usage_map_.end(),
                 std::format("Unrecognized image '{}'", image_name));
    return iter->second;
  }

  ImageUsageTracker& UpdateUsage(const std::string& image_name,
                                 const ImageUsage& usage) {
    auto iter = image_usage_map_.find(image_name);
    ASSERT_FALSE(iter == image_usage_map_.end(),
                 std::format("Unrecognized image '{}'", image_name));
    iter->second = usage;
    return *this;
  }

 private:
  std::unordered_map<std::string, ImageUsage> image_usage_map_;
};

// The base class of compute pass and graphics pass.
class BasePass {
 public:
  // Maps image name to usage history.
  using ImageUsageHistoryMap =
      std::unordered_map<std::string, ImageUsageHistory>;

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
                               ImageUsageTracker& usage_tracker) const;

 protected:
  // Holds the previous and current image usage.
  struct ImageUsagesInfo {
    ImageUsagesInfo(int prev_usage_subpass, const ImageUsage* prev_usage,
                    const ImageUsage* curr_usage)
        : prev_usage_subpass{prev_usage_subpass},
          prev_usage{*FATAL_IF_NULL(prev_usage)},
          curr_usage{*FATAL_IF_NULL(curr_usage)} {}

    const int prev_usage_subpass;
    const ImageUsage& prev_usage;
    const ImageUsage& curr_usage;
  };

  // Adds an image that is used in this pass. This checks whether subpasses
  // stored in the history are out of range.
  void AddUsageHistory(std::string&& image_name, ImageUsageHistory&& history);

  // Returns the usage history of image.
  const ImageUsageHistory& GetUsageHistory(const std::string& image_name) const;

  // Returns a pointer to image usage at 'subpass', or nullptr if the usage has
  // not been specified for that subpass.
  const ImageUsage* GetImageUsage(const std::string& image_name,
                                  int subpass) const;

  // Returns previous and current image usages info if the image is used at
  // 'subpass' and synchronization on image memory access is needed,
  std::optional<ImageUsagesInfo> GetImageUsagesIfNeedSynchronization(
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
