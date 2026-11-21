//
//  image_util.h
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_IMAGE_UTIL_H
#define LIGHTER_RENDERER_VK_IMAGE_UTIL_H

#include <optional>
#include <span>

#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk::image {

// Returns VkAccessFlags2 used for inserting image memory barriers.
intl::AccessFlags2 GetAccessFlags(const ir::ImageUsage& usage);

// Returns VkPipelineStageFlags2 used for inserting image memory barriers.
intl::PipelineStageFlags2 GetPipelineStageFlags(const ir::ImageUsage& usage);

// Returns which VkImageLayout should be used for 'usage'.
intl::ImageLayout GetImageLayout(const ir::ImageUsage& usage);

// Returns VkImageUsageFlags that contains all usages.
intl::ImageUsageFlags GetImageUsageFlags(
    std::span<const ir::ImageUsage> usages);

// Returns the family index of the queue that accesses the image for 'usage'.
// Note that since this is used for creating image buffers, it will return
// std::nullopt for the following usage types:
// - kDontCare.
// - kPresentation and kTransfer, since the queue should be inferred from
//   previous or next usages. Note that both graphics and compute queues can
//   write to swapchain and do transfer.
std::optional<uint32_t> GetQueueFamilyIndex(const Context& context,
                                            const ir::ImageUsage& usage);

// Returns whether an explicit barrier is needed when transitioning from
// 'prev_usage' to 'curr_usage'.
bool NeedsSynchronization(const ir::ImageUsage& prev_usage,
                          const ir::ImageUsage& curr_usage);

// Synthesizes a Synchronization2 ImageMemoryBarrier2 transitioning 'image'
// across 'subresource_range' from 'prev_usage' to 'curr_usage'. Returns
// std::nullopt if no synchronization or layout transition is needed.
std::optional<intl::ImageMemoryBarrier2> CreateImageMemoryBarrier(
    intl::Image image, const intl::ImageSubresourceRange& subresource_range,
    const ir::ImageUsage& prev_usage, const ir::ImageUsage& curr_usage,
    uint32_t src_queue_family_index = VK_QUEUE_FAMILY_IGNORED,
    uint32_t dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED);

// Creates a VkImageView for 'vk_image'.
intl::ImageView CreateImageView(const Context& context, intl::Image vk_image,
                                intl::ImageViewType view_type,
                                intl::Format format,
                                intl::ImageAspectFlags aspect_mask,
                                uint32_t mip_levels, uint32_t layer_count);

// Records an ImageMemoryBarrier2 via pipelineBarrier2 on 'cmd'.
void TransitionImageLayout(intl::CommandBuffer cmd, intl::Image image,
                           intl::ImageAspectFlags aspect_mask,
                           uint32_t base_mip_level, uint32_t level_count,
                           uint32_t layer_count, intl::ImageLayout old_layout,
                           intl::ImageLayout new_layout,
                           intl::PipelineStageFlags2 src_stage_mask,
                           intl::AccessFlags2 src_access_mask,
                           intl::PipelineStageFlags2 dst_stage_mask,
                           intl::AccessFlags2 dst_access_mask);

}  // namespace lighter::renderer::vk::image

#endif  // LIGHTER_RENDERER_VK_IMAGE_UTIL_H
