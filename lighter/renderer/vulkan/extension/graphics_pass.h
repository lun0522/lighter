//
//  graphics_pass.h
//
//  Created by Pujun Lun on 6/27/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H

#include <string>
#include <vector>

#include "lighter/renderer/vulkan/wrapper/image.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/optional.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// Forward declarations.
class GraphicsPass;

// This class holds usages of an image in different subpasses of a graphics
// pass, which is used to construct render passes.
class ImageGraphicsUsageHistory {
 public:
  ImageGraphicsUsageHistory(std::string&& image_name, int num_subpasses)
      : image_name_{std::move(image_name)} {
    usage_history_.resize(num_subpasses + 1);
  }

  // This class is only movable.
  ImageGraphicsUsageHistory(ImageGraphicsUsageHistory&&) noexcept = default;
  ImageGraphicsUsageHistory& operator=(ImageGraphicsUsageHistory&&) noexcept
      = default;

 private:
  friend class GraphicsPass;

  // Name of image.
  std::string image_name_;

  // Image usages indexed by subpass. The length of this vector is the number of
  // subpasses plus 1, where the last element represents the usage after this
  // graphics pass.
  std::vector<absl::optional<image::Usage>> usage_history_;
};

class GraphicsPass {
 public:
  explicit GraphicsPass(int num_subpasses) : num_subpasses_{num_subpasses} {}

  // This class is neither copyable nor movable.
  GraphicsPass(const GraphicsPass&) = delete;
  GraphicsPass& operator=(const GraphicsPass&) = delete;

  // Adds an image that is used in this graphics pass, along with its usage
  // history.
  GraphicsPass& AddImage(Image* image, ImageGraphicsUsageHistory&& history);

 private:
  // Number of subpasses.
  const int num_subpasses_;

  // Maps each image to its usage history.
  absl::flat_hash_map<Image*, ImageGraphicsUsageHistory>
      image_usage_history_map_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H */
