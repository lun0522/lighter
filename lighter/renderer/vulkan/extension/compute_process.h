//
//  compute_process.h
//
//  Created by Pujun Lun on 6/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_COMPUTE_PROCESS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_COMPUTE_PROCESS_H

#include <map>
#include <string>
#include <vector>

#include "lighter/renderer/vulkan/wrapper/image.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {

class ImageUsageHistory {
 public:
  ImageUsageHistory(const Image* image, std::string&& image_name,
                    int num_stages);

  // This class is neither copyable nor movable.
  ImageUsageHistory(const ImageUsageHistory&) = delete;
  ImageUsageHistory& operator=(const ImageUsageHistory&) = delete;

  // Specifies the usage of image at 'stage'. This should be called only if no
  // usage has been added for 'stage'.
  void AddUsageAtStage(int stage, const image::Usage& usage);

  // Returns true if the image is used at 'stage'.
  bool IsImageUsedAtStage(int stage) const {
    return usage_at_stage_map_.find(stage) != usage_at_stage_map_.end();
  }

 private:
  // Validates that 'stage' is within range [0, 'num_stages_' - 1].
  void ValidateStage(int stage) const;

  // Image managed by this manager.
  const Image& image_;

  // Name of image, only used for debugging.
  const std::string image_name_;

  // Number of stages.
  const int num_stages_;

  // Maps stages where the image is used to its usage at that stage. An ordered
  // map is used so that we can look up the previous usage efficiently.
  std::map<int, image::Usage> usage_at_stage_map_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_COMPUTE_PROCESS_H */
