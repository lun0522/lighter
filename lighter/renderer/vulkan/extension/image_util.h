//
//  image_util.h
//
//  Created by Pujun Lun on 7/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_IMAGE_UTIL_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_IMAGE_UTIL_H

#include <map>
#include <string>
#include <vector>

#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/image_usage.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/absl/container/flat_hash_map.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace image {

// This class tracks usages of multiple images. Each image should have a unique
// name as identifier.
class UsageTracker {
 public:
  UsageTracker() = default;

  // This class is neither copyable nor movable.
  UsageTracker(const UsageTracker&) = delete;
  UsageTracker& operator=(const UsageTracker&) = delete;

  // Makes this tracker track the usage of image.
  UsageTracker& TrackImage(std::string&& image_name,
                           const Usage& current_usage);
  UsageTracker& TrackImage(const std::string& image_name,
                           const Usage& current_usage) {
    return TrackImage(std::string{image_name}, current_usage);
  }

  // Makes this tracker track the usage of image, assuming its initial usage is
  // its current usage.
  UsageTracker& TrackImage(std::string&& image_name,
                           const Image& sample_image) {
    return TrackImage(std::move(image_name), sample_image.GetInitialUsage());
  }
  UsageTracker& TrackImage(const std::string& image_name,
                           const Image& sample_image) {
    return TrackImage(std::string{image_name}, sample_image);
  }

  // Returns true if the usage of image is being tracked.
  bool IsImageTracked(const std::string& image_name) const {
    return image_usage_map_.contains(image_name);
  }

  // Returns the current usage of image.
  const Usage& GetUsage(const std::string& image_name) const;

  // Updates the current usage of image.
  UsageTracker& UpdateUsage(const std::string& image_name, const Usage& usage);

 private:
  // Maps the image name to the current usage of that image.
  absl::flat_hash_map<std::string, Usage> image_usage_map_;
};

// This class holds usages of an image in subpasses of a compute pass or
// graphics pass. We assume that an image can only have one usage at a subpass.
class UsageHistory {
 public:
  explicit UsageHistory(const Usage& initial_usage)
      : initial_usage_{initial_usage} {}

  // This constructor should be used only if the image has not been constructed
  // yet. In that case, the user should add all usages throughout the entire
  // lifetime of the image to this history.
  explicit UsageHistory() = default;

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

  // Returns all usages at all subpasses, including the initial and final usages
  // if specified. Note that this may contain duplicates.
  std::vector<Usage> GetAllUsages() const;

  // Accessors.
  const std::map<int, Usage>& usage_at_subpass_map() const {
    return usage_at_subpass_map_;
  }
  const Usage& initial_usage() const { return initial_usage_; }
  const absl::optional<Usage>& final_usage() const { return final_usage_; }

 private:
  // Maps subpasses where the image is used to its usage at that subpass. An
  // ordered map is used to look up the previous/next usage efficiently.
  std::map<int, Usage> usage_at_subpass_map_;

  // Usage of image before this pass.
  Usage initial_usage_;

  // Usage of image after this pass.
  absl::optional<Usage> final_usage_;
};

} /* namespace image */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_IMAGE_UTIL_H */
