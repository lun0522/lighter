//
//  image_util.h
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_IMAGE_UTIL_H
#define LIGHTER_RENDERER_VK_IMAGE_UTIL_H

#include <optional>

#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/vk/context.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter::renderer::vk::image {

// Returns VkAccessFlags used for inserting image memory barriers.
VkAccessFlags GetAccessFlags(const ImageUsage& usage);

// Returns VkPipelineStageFlags used for inserting image memory barriers.
VkPipelineStageFlags GetPipelineStageFlags(const ImageUsage& usage);

// Returns which VkImageLayout should be used for 'usage'.
VkImageLayout GetImageLayout(const ImageUsage& usage);

// Returns VkImageUsageFlags that contains all usages.
VkImageUsageFlags GetImageUsageFlags(absl::Span<const ImageUsage> usages);

// Returns the family index of the queue that accesses the image for 'usage'.
// Note that since this is used for creating image buffers, it will return
// std::nullopt for the following usage types:
// - kDontCare.
// - kPresentation and kTransfer, since the queue should be inferred from
//   previous or next usages. Note that both graphics and compute queues can
//   write to swapchain and do transfer.
std::optional<uint32_t> GetQueueFamilyIndex(const Context& context,
                                            const ImageUsage& usage);

// Returns whether we need to explicitly synchronize image memory access when
// the image usage changes, which means to insert memory barriers in compute
// pass, or add subpass dependencies in graphics pass.
bool NeedSynchronization(const ImageUsage& prev_usage,
                         const ImageUsage& curr_usage);

}  // namespace vk::renderer::lighter::image

#endif  // LIGHTER_RENDERER_VK_IMAGE_UTIL_H
