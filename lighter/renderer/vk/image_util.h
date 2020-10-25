//
//  image_util.h
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_IMAGE_UTIL_H
#define LIGHTER_RENDERER_VK_IMAGE_UTIL_H

#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/vk/context.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace image {

// Returns VkAccessFlags used for inserting image memory barriers.
VkAccessFlags GetAccessFlags(const ImageUsage& usage);

// Returns VkPipelineStageFlags used for inserting image memory barriers.
VkPipelineStageFlags GetPipelineStageFlags(const ImageUsage& usage);

// Returns which VkImageLayout should be used for 'usage'.
VkImageLayout GetImageLayout(const ImageUsage& usage);

// Returns VkImageUsageFlagBits for 'usage'. Note that this must not be called
// if usage type is kDontCare, since it doesn't have corresponding flag bits.
VkImageUsageFlagBits GetImageUsageFlagBits(const ImageUsage& usage);

// Returns VkImageUsageFlags that contains all usages.
VkImageUsageFlags GetImageUsageFlags(absl::Span<const ImageUsage> usages);

// Returns the family index of the queue that accesses the image for 'usage'.
// Note that since this is used for creating image buffers, it will return
// absl::nullopt for the following usage types (apart from kDontCare):
// - kPresentation, since we don't create swapchain image buffers by ourselves.
// - kTransfer, since the queue should be inferred from previous or next usages.
absl::optional<uint32_t> GetQueueFamilyIndex(const Context& context,
                                             const ImageUsage& usage);

// Returns whether we need to explicitly synchronize image memory access when
// the image usage changes, which means to insert memory barriers in compute
// pass, or add subpass dependencies in graphics pass.
bool NeedSynchronization(const ImageUsage& prev_usage,
                         const ImageUsage& curr_usage);

} /* namespace image */
} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_IMAGE_UTIL_H */
