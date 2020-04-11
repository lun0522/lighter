//
//  image_util.h
//
//  Created by Pujun Lun on 4/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_UTIL_H

#include <vector>
#include <string>

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

  struct UsageInfo {
    struct UsageAtStage {
      ImageUsage usage;
      int stage;
    };

    UsageInfo(const VkImage* image, std::string&& image_name,
              VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED)
        : image{FATAL_IF_NULL(image)}, image_name{std::move(image_name)},
          initial_layout{initial_layout} {}

    UsageInfo& AddUsage(int stage, ImageUsage usage) {
      usages.emplace_back(UsageAtStage{usage, stage});
      return *this;
    }

    const VkImage* image;
    const std::string image_name;
    const VkImageLayout initial_layout;
    std::vector<UsageAtStage> usages;
  };

  ImageLayoutManager(int num_stages, absl::Span<const UsageInfo> infos);

  // This class is neither copyable nor movable.
  ImageLayoutManager(const ImageLayoutManager&) = delete;
  ImageLayoutManager& operator=(const ImageLayoutManager&) = delete;

  VkImageLayout GetImageLayoutAtStage(const VkImage& image, int stage) const;

  bool NeedMemoryBarrierBeforeStage(int stage) const;

  void InsertMemoryBarrierBeforeStage(const VkCommandBuffer& command_buffer,
                                      uint32_t queue_family_index,
                                      int stage) const;

 private:
  void ValidateStage(int stage) const;

  void BuildUsageMap(const UsageInfo& info);

  const int num_stages_;

  absl::flat_hash_map<const VkImage*, std::vector<VkImageLayout>>
      image_layouts_map_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_UTIL_H */
