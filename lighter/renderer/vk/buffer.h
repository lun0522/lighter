//
//  buffer.h
//
//  Created by Pujun Lun on 10/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_BUFFER_H
#define LIGHTER_RENDERER_VK_BUFFER_H

#include "lighter/common/util.h"
#include "lighter/renderer/buffer.h"
#include "lighter/renderer/buffer_usage.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {
namespace vk {

class DeviceBuffer : public renderer::DeviceBuffer {
 public:
  DeviceBuffer(SharedContext context, UpdateRate update_rate,
               size_t initial_size, absl::Span<const BufferUsage> usages)
      : context_{std::move(FATAL_IF_NULL(context))},
        allocation_info_{*context_, update_rate, usages} {
    AllocateBufferAndMemory(initial_size);
  }

  // This class is neither copyable nor movable.
  DeviceBuffer(const DeviceBuffer&) = delete;
  DeviceBuffer& operator=(const DeviceBuffer&) = delete;

  ~DeviceBuffer() { DeallocateBufferAndMemory(); }

 private:
  struct AllocationInfo {
    AllocationInfo(const Context& context, UpdateRate update_rate,
                   absl::Span<const BufferUsage> usages);

    VkBufferUsageFlags usage_flags = nullflag;
    VkMemoryPropertyFlags memory_property_flags = nullflag;
    VkSharingMode sharing_mode;
    std::vector<uint32_t> unique_queue_family_indices;
  };

  void AllocateBufferAndMemory(size_t size);

  void DeallocateBufferAndMemory();

  const SharedContext context_;

  const AllocationInfo allocation_info_;

  size_t buffer_size_ = 0;

  // Opaque buffer object.
  VkBuffer buffer_;

  // TODO: Hold multiple buffers in one block of device memory.
  // Opaque device memory object.
  VkDeviceMemory device_memory_;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_BUFFER_H */
