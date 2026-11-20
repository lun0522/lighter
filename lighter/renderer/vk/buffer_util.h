//
//  buffer_util.h
//
//  Created by Pujun Lun on 11/3/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_BUFFER_UTIL_H
#define LIGHTER_RENDERER_VK_BUFFER_UTIL_H

#include <vk_mem_alloc.h>

#include <optional>
#include <span>
#include <vector>

#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk::buffer {

// Holds a VkBuffer together with its VMA memory allocation.
struct AllocatedBuffer {
  intl::Buffer handle;
  VmaAllocation allocation = VK_NULL_HANDLE;
};

// Returns VkBufferUsageFlags that contains all usages.
intl::BufferUsageFlags GetBufferUsageFlags(
    std::span<const ir::BufferUsage> usages);

// Returns the family index of the queue that accesses the buffer for 'usage'.
// Note that since this is used for creating buffers, it will return
// std::nullopt for the following usage types (apart from kDontCare):
// - kTransfer, since the queue should be inferred from previous or next usages.
std::optional<uint32_t> GetQueueFamilyIndex(const Context& context,
                                            const ir::BufferUsage& usage);

// Allocates a buffer and its backing memory via VMA.
AllocatedBuffer CreateBuffer(
    const Context& context, intl::DeviceSize size,
    intl::BufferUsageFlags usage_flags,
    const std::vector<uint32_t>& unique_queue_family_indices,
    bool host_visible);

// Allocates a host-visible transfer source staging buffer via VMA.
AllocatedBuffer CreateStagingBuffer(const Context& context,
                                    intl::DeviceSize size);

// Destroys 'buffer' and frees its VMA allocation.
void DestroyBuffer(const Context& context, const AllocatedBuffer& buffer);

// Copies host memory regions specified by 'copy_infos' into a host-visible
// VMA allocation and flushes caches.
void CopyHostDataToAllocation(const Context& context, VmaAllocation allocation,
                              std::span<const ir::Buffer::CopyInfo> copy_infos);

}  // namespace lighter::renderer::vk::buffer

#endif  // LIGHTER_RENDERER_VK_BUFFER_UTIL_H
