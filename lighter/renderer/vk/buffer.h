//
//  buffer.h
//
//  Created by Pujun Lun on 10/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_BUFFER_H
#define LIGHTER_RENDERER_VK_BUFFER_H

#include <vk_mem_alloc.h>

#include <memory>
#include <span>

#include "lighter/common/util.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

class Buffer : public WithSharedContext, public ir::Buffer {
 public:
  static std::unique_ptr<Buffer> CreateTransient(
      const SharedContext& context, size_t size,
      intl::BufferUsageFlags usage_flags);

  Buffer(const SharedContext& context, UpdateRate update_rate,
         size_t initial_size, std::span<const ir::BufferUsage> usages)
      : WithSharedContext{context},
        allocation_info_{*context_, update_rate, usages} {
    AllocateBufferAndMemory(initial_size);
  }

  // This class is neither copyable nor movable.
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  ~Buffer() override { DeallocateBufferAndMemory(); }

  void CopyToDevice(std::span<const CopyInfo> copy_infos) const override;

  // Accessors.
  intl::Buffer vk_buffer() const { return buffer_; }
  size_t size() const { return buffer_size_; }

 private:
  struct AllocationInfo {
    AllocationInfo(const Context& context, UpdateRate update_rate,
                   std::span<const ir::BufferUsage> usages);
    AllocationInfo(const Context& context, intl::BufferUsageFlags usage_flags,
                   bool host_visible);

    bool host_visible = false;
    intl::BufferUsageFlags usage_flags;
    std::vector<uint32_t> unique_queue_family_indices;
  };

  Buffer(const SharedContext& context, size_t initial_size,
         intl::BufferUsageFlags usage_flags)
      : WithSharedContext{context},
        allocation_info_{*context_, usage_flags, /*host_visible=*/false} {
    AllocateBufferAndMemory(initial_size);
  }

  void AllocateBufferAndMemory(size_t size);

  void DeallocateBufferAndMemory();

  const AllocationInfo allocation_info_;

  size_t buffer_size_ = 0;

  // Opaque buffer object.
  intl::Buffer buffer_;

  // VMA allocation handle.
  VmaAllocation allocation_ = VK_NULL_HANDLE;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_BUFFER_H
