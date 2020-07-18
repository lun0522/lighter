//
//  base_pass.h
//
//  Created by Pujun Lun on 7/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_BASE_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_BASE_PASS_H

#include <map>
#include <string>
#include <vector>

#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/image_util.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/optional.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace image {

// This class holds usages of an image in subpasses of a compute pass or
// graphics pass. We assume that an image can only have one usage at a subpass.
class UsageHistory {
 public:
  explicit UsageHistory(std::string&& image_name)
      : image_name_{std::move(image_name)} {}

  // This class is only movable.
  UsageHistory(UsageHistory&&) noexcept = default;
  UsageHistory& operator=(UsageHistory&&) noexcept = default;

  // Specifies the usage at 'subpass'.
  UsageHistory& AddUsage(int subpass, const Usage& usage);

  // Specifies the same usage for all subpasses in range
  // ['subpass_start', 'subpass_end'].
  UsageHistory& AddUsage(int subpass_start, int subpass_end,
                         const Usage& usage);

  // Specifies the usage after this pass. This is optional. It should be called
  // only if the user wants to explicitly transition the image layout to prepare
  // for later operations.
  UsageHistory& SetFinalUsage(const Usage& usage);

  // Returns the image usage at 'subpass'. The usage must have been specified
  // via AddUsage().
  const Usage& GetUsage(int subpass) const;

  // Returns all usages at all subpasses, including the final usage if
  // specified. Note that this may contain duplicates.
  std::vector<Usage> GetAllUsages() const;

  // Checks whether all subpasses specified are in range [0, 'upper_bound').
  void ValidateSubpasses(int upper_bound) const;

  // Accessors.
  const std::string& image_name() const { return image_name_; }
  const std::map<int, Usage>& usage_at_subpass_map() const {
    return usage_at_subpass_map_;
  }
  const absl::optional<Usage>& final_usage() const { return final_usage_; }

 private:
  // Name of image.
  std::string image_name_;

  // Maps subpasses where the image is used to its usage at that subpass. An
  // ordered map is used to look up the previous/next usage efficiently.
  std::map<int, Usage> usage_at_subpass_map_;

  // Usage of image after this pass.
  absl::optional<Usage> final_usage_;
};

} /* namespace image */

// The base class of compute pass and graphics pass.
class BasePass {
 public:
  explicit BasePass(int num_subpasses) : num_subpasses_{num_subpasses} {}

  // This class is neither copyable nor movable.
  BasePass(const BasePass&) = delete;
  BasePass& operator=(const BasePass&) = delete;

  virtual ~BasePass() = default;

  // Adds an image that is used in this pass, along with its usage history.
  // The current usage of 'image' will be used as the initial usage.
  BasePass& AddImage(Image* image, image::UsageHistory&& history);

  // Returns the layout of 'image' at 'subpass'. The usage at this subpass must
  // have been specified in the usage history.
  VkImageLayout GetImageLayoutAtSubpass(const Image& image, int subpass) const;

 protected:
  // Accessors.
  const absl::flat_hash_map<Image*, image::UsageHistory>&
  image_usage_history_map() const { return image_usage_history_map_; }

  // Number of subpasses.
  const int num_subpasses_;

 private:
  // Maps each image to its usage history.
  absl::flat_hash_map<Image*, image::UsageHistory> image_usage_history_map_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_BASE_PASS_H */
