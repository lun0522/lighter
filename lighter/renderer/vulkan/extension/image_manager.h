//
//  image_manager.h
//
//  Created by Pujun Lun on 6/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_IMAGE_MANAGER_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_IMAGE_MANAGER_H

#include <map>
#include <vector>

#include "lighter/renderer/vulkan/wrapper/image.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {
namespace vulkan {

class ImageUsageManager {
 public:
  ImageUsageManager(const Image* image, int num_stages,
                    absl::Span<const image::UsageAtStage> usage_at_stages);

  // This class is neither copyable nor movable.
  ImageUsageManager(const ImageUsageManager&) = delete;
  ImageUsageManager& operator=(const ImageUsageManager&) = delete;

 private:
  // Validates that 'stage' is within range [0, 'num_stages_' - 1].
  void ValidateStage(int stage) const;

  // Image managed by this manager.
  const Image& image_;

  // Number of stages.
  const int num_stages_;

  // Maps stages where the image is used to its usage at that stage. An ordered
  // map is used so that we can look up the previous usage efficiently.
  std::map<int, image::Usage> usage_at_stage_map_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_IMAGE_MANAGER_H */
