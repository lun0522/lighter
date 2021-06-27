//
//  buffer_util.h
//
//  Created by Pujun Lun on 11/3/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_BUFFER_UTIL_H
#define LIGHTER_RENDERER_VK_BUFFER_UTIL_H

#include <optional>

#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk::buffer {

// Returns VkBufferUsageFlags that contains all usages.
intl::BufferUsageFlags GetBufferUsageFlags(
    absl::Span<const ir::BufferUsage> usages);

// Returns the family index of the queue that accesses the buffer for 'usage'.
// Note that since this is used for creating buffers, it will return
// std::nullopt for the following usage types (apart from kDontCare):
// - kTransfer, since the queue should be inferred from previous or next usages.
std::optional<uint32_t> GetQueueFamilyIndex(const Context& context,
                                            const ir::BufferUsage& usage);

// Allocates device memory.
intl::DeviceMemory CreateDeviceMemory(
    const Context& context, const intl::MemoryRequirements& requirements,
    intl::MemoryPropertyFlags property_flags);

// Deallocates device memory.
inline void FreeDeviceMemory(const Context& context,
                             intl::DeviceMemory device_memory) {
  context.device()->freeMemory(device_memory, *context.host_allocator());
}

}  // namespace lighter::renderer::vk::buffer

#endif  // LIGHTER_RENDERER_VK_BUFFER_UTIL_H
