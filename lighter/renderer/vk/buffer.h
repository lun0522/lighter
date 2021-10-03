//
//  buffer.h
//
//  Created by Pujun Lun on 10/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_BUFFER_H
#define LIGHTER_RENDERER_VK_BUFFER_H

#include "lighter/common/util.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk {

class Buffer : public WithSharedContext,
               public ir::Buffer {
 public:
  Buffer(const SharedContext& context, UpdateRate update_rate,
         size_t initial_size, absl::Span<const ir::BufferUsage> usages)
      : WithSharedContext{context},
        allocation_info_{*context_, update_rate, usages} {
    AllocateBufferAndMemory(initial_size);
  }

  // This class is neither copyable nor movable.
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  ~Buffer() override { DeallocateBufferAndMemory(); }

  void CopyToDevice(absl::Span<const CopyInfo> copy_infos) const override;

 private:
  struct AllocationInfo {
    AllocationInfo(const Context& context, UpdateRate update_rate,
                   absl::Span<const ir::BufferUsage> usages);

    // Returns true if the device memory is visible to host.
    bool IsHostVisible() const {
      return static_cast<bool>(
          memory_property_flags & intl::MemoryPropertyFlagBits::eHostVisible);
    }

    intl::BufferUsageFlags usage_flags;
    intl::MemoryPropertyFlags memory_property_flags;
    std::vector<uint32_t> unique_queue_family_indices;
  };

  void AllocateBufferAndMemory(size_t size);

  void DeallocateBufferAndMemory();

  const AllocationInfo allocation_info_;

  size_t buffer_size_ = 0;

  // Opaque buffer object.
  intl::Buffer buffer_;

  // TODO: Integrate VMA.
  // Opaque device memory object.
  intl::DeviceMemory device_memory_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_BUFFER_H
