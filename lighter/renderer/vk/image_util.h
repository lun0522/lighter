//
//  image_util.h
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_IMAGE_UTIL_H
#define LIGHTER_RENDERER_VK_IMAGE_UTIL_H

#include <optional>

#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk::image {

// Returns VkAccessFlags used for inserting image memory barriers.
intl::AccessFlags GetAccessFlags(const ir::ImageUsage& usage);

// Returns VkPipelineStageFlags used for inserting image memory barriers.
intl::PipelineStageFlags GetPipelineStageFlags(const ir::ImageUsage& usage);

// Returns which VkImageLayout should be used for 'usage'.
intl::ImageLayout GetImageLayout(const ir::ImageUsage& usage);

// Returns VkImageUsageFlags that contains all usages.
intl::ImageUsageFlags GetImageUsageFlags(
    absl::Span<const ir::ImageUsage> usages);

// Returns the family index of the queue that accesses the image for 'usage'.
// Note that since this is used for creating image buffers, it will return
// std::nullopt for the following usage types:
// - kDontCare.
// - kPresentation and kTransfer, since the queue should be inferred from
//   previous or next usages. Note that both graphics and compute queues can
//   write to swapchain and do transfer.
std::optional<uint32_t> GetQueueFamilyIndex(const Context& context,
                                            const ir::ImageUsage& usage);

// Returns whether we need to explicitly synchronize image memory access when
// the image usage changes, which means to insert memory barriers in compute
// pass, or add subpass dependencies in graphics pass.
bool NeedsSynchronization(const ir::ImageUsage& prev_usage,
                          const ir::ImageUsage& curr_usage);

}  // namespace lighter::renderer::vk::image

#endif  // LIGHTER_RENDERER_VK_IMAGE_UTIL_H
