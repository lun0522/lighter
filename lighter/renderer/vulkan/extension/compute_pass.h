//
//  compute_pass.h
//
//  Created by Pujun Lun on 6/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_COMPUTE_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_COMPUTE_PASS_H

#include <functional>
#include <string>

#include "lighter/renderer/vulkan/extension/base_pass.h"
#include "lighter/renderer/vulkan/extension/image_usage_util.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// This class is used to analyze usages of images involved in a sequence of
// compute shader invocations, so that we can insert memory barriers to
// transition image layouts whenever necessary. One subpass may contain several
// compute shader invocations, and we won't insert barriers in the middle.
class ComputePass : public BasePass {
 public:
  // Specifies compute operations to perform in one subpass.
  using ComputeOp = std::function<void()>;

  // Inherits constructor.
  using BasePass::BasePass;

  // This class is neither copyable nor movable.
  ComputePass(const ComputePass&) = delete;
  ComputePass& operator=(const ComputePass&) = delete;

  // Adds an image that is used in this compute pass.
  ComputePass& AddImage(std::string&& image_name,
                        image::UsageHistory&& history);
  ComputePass& AddImage(const std::string& image_name,
                        image::UsageHistory&& history) {
    return AddImage(std::string{image_name}, std::move(history));
  }

  // Runs 'compute_ops' and inserts memory barriers internally for transitioning
  // image layouts using the queue with 'queue_family_index'.
  // 'image_map' should include all images used in this compute pass.
  // The size of 'compute_ops' must be equal to the number of subpasses.
  // This should be called when 'command_buffer' is recording commands.
  // TODO: Handle queue transfer
  void Run(const VkCommandBuffer& command_buffer, uint32_t queue_family_index,
           const absl::flat_hash_map<std::string, const Image*>& image_map,
           absl::Span<const ComputeOp> compute_ops) const;

 private:
  // Inserts a memory barrier for transitioning the layout of 'image' using the
  // queue with 'queue_family_index'.
  void InsertMemoryBarrier(const VkCommandBuffer& command_buffer,
                           uint32_t queue_family_index, const VkImage& image,
                           const image::Usage& prev_usage,
                           const image::Usage& curr_usage) const;

  // Checks whether image usages recorded in 'history' (excluding initial and
  // final usages) can be handled by this compute pass.
  void ValidateUsageHistory(const std::string& image_name,
                            const image::UsageHistory& history) const;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_COMPUTE_PASS_H */
