//
//  buffer_util.h
//
//  Created by Pujun Lun on 11/3/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_BUFFER_UTIL_H
#define LIGHTER_RENDERER_VK_BUFFER_UTIL_H

#include "lighter/renderer/buffer_usage.h"
#include "lighter/renderer/vk/context.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace buffer {

// Returns VkBufferUsageFlags that contains all usages.
VkBufferUsageFlags GetBufferUsageFlags(absl::Span<const BufferUsage> usages);

// Returns the family index of the queue that accesses the buffer for 'usage'.
// Note that since this is used for creating buffers, it will return
// absl::nullopt for the following usage types (apart from kDontCare):
// - kTransfer, since the queue should be inferred from previous or next usages.
absl::optional<uint32_t> GetQueueFamilyIndex(const Context& context,
                                             const BufferUsage& usage);

// Allocates device memory.
VkDeviceMemory CreateDeviceMemory(
    const Context& context, const VkMemoryRequirements& requirements,
    VkMemoryPropertyFlags property_flags);

// Deallocates device memory.
inline void FreeDeviceMemory(const Context& context,
                             const VkDeviceMemory& device_memory) {
  vkFreeMemory(*context.device(), device_memory, *context.host_allocator());
}

} /* namespace buffer */
} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_BUFFER_UTIL_H */
