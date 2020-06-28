//
//  compute_pass.h
//
//  Created by Pujun Lun on 6/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_COMPUTE_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_COMPUTE_PASS_H

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "lighter/renderer/vulkan/wrapper/image.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// Forward declarations.
class ComputePass;

// This class holds usages of an image in different stages of a compute pass.
class ImageUsageHistory {
 public:
  ImageUsageHistory(std::string&& image_name, int num_stages)
      : image_name_{std::move(image_name)}, num_stages_{num_stages} {}

  // This class is only movable.
  ImageUsageHistory(ImageUsageHistory&&) noexcept = default;
  ImageUsageHistory& operator=(ImageUsageHistory&&) noexcept = default;

  // Specifies the usage at 'stage'. Note that this can be called only once for
  // each stage, since we assume an image can have only one usage at each stage.
  ImageUsageHistory& AddUsage(int stage, const image::Usage& usage);

  // Specifies the usage after this compute pass. This is optional. It should be
  // called only if the user wants to transition the image layout to prepare for
  // later operations. Also note that this can be called only once, since an
  // image can only have one final usage.
  ImageUsageHistory& SetFinalUsage(const image::Usage& usage);

 private:
  friend class ComputePass;

  // Name of image.
  std::string image_name_;

  // Number of stages.
  int num_stages_;

  // Maps stages where the image is used to its usage at that stage. An
  // ordered map is used to look up the previous/next usage efficiently.
  std::map<int, image::Usage> usage_at_stage_map_;

  // Usage of image after this compute pass.
  absl::optional<image::Usage> final_usage_;
};

// This class is used to analyze usages of images involved in a sequence of
// compute shader invocations, so that we can insert memory barriers to
// transition image layouts whenever necessary. Expected usage:
//   ComputePass compute_pass(...);
//   compute_pass.GetImageUsageHistory(...).AddUsage(...).SetFinalUsage(...);
//   compute_pass.Run(...);
class ComputePass {
 public:
  // Specifies compute operations to perform in one stage.
  using ComputeOp = std::function<void()>;

  // Holds info of an image.
  struct ImageInfo {
    Image* image;
    std::string name;
  };

  ComputePass(int num_stages, absl::Span<ImageInfo> image_infos);

  // This class is neither copyable nor movable.
  ComputePass(const ComputePass&) = delete;
  ComputePass& operator=(const ComputePass&) = delete;

  // Returns reference to the usage history of 'image', with which the user can
  // add usages for different stages before calling Run().
  ImageUsageHistory& GetImageUsageHistory(const Image& image);

  // Runs 'compute_ops' and inserts memory barriers internally for transitioning
  // image layouts using the queue with 'queue_family_index'.
  // The size of 'compute_ops' must be equal to the number of stages.
  // This should be called when 'command_buffer' is recording commands.
  // TODO: Handle queue transfer
  void Run(const VkCommandBuffer& command_buffer, uint32_t queue_family_index,
           absl::Span<const ComputeOp> compute_ops) const;

 private:
  // Inserts a memory barrier for transitioning the layout of 'image' using the
  // queue with 'queue_family_index'.
  void InsertMemoryBarrier(const VkCommandBuffer& command_buffer,
                           uint32_t queue_family_index, const VkImage& image,
                           image::Usage prev_usage,
                           image::Usage next_usage) const;

  // Number of stages.
  const int num_stages_;

  // Maps each image to its usage history.
  absl::flat_hash_map<Image*, ImageUsageHistory> image_usage_history_map_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_COMPUTE_PASS_H */
