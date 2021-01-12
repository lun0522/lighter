//
//  buffer.cc
//
//  Created by Pujun Lun on 10/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/buffer.h"

#include <algorithm>
#include <cstring>

#include "lighter/renderer/vk/buffer_util.h"
#include "third_party/absl/container/flat_hash_set.h"

namespace lighter::renderer::vk {
namespace {

using CopyInfo = renderer::DeviceBuffer::CopyInfo;

// Creates a buffer of 'data_size'.
VkBuffer CreateBuffer(const Context& context, VkDeviceSize data_size,
                      VkBufferUsageFlags usage_flags,
                      absl::Span<const uint32_t> unique_queue_family_indices) {
  const VkBufferCreateInfo buffer_info{
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      data_size,
      usage_flags,
      VK_SHARING_MODE_EXCLUSIVE,
      CONTAINER_SIZE(unique_queue_family_indices),
      unique_queue_family_indices.data(),
  };

  VkBuffer buffer;
  ASSERT_SUCCESS(vkCreateBuffer(*context.device(), &buffer_info,
                                *context.host_allocator(), &buffer),
                 "Failed to create buffer");
  return buffer;
}

// Destroys "buffer".
void DestroyBuffer(const Context& context, const VkBuffer& buffer) {
  vkDestroyBuffer(*context.device(), buffer, *context.host_allocator());
}

// Allocates device memory for 'buffer' with 'memory_properties'.
VkDeviceMemory CreateBufferMemory(const Context& context,
                                  const VkBuffer& buffer,
                                  VkMemoryPropertyFlags property_flags) {
  const VkDevice& device = *context.device();
  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(device, buffer, &requirements);

  VkDeviceMemory device_memory = buffer::CreateDeviceMemory(
      context, requirements, property_flags);

  // Bind the allocated memory with 'buffer'. If this memory is used for
  // multiple buffers, the memory offset should be re-calculated and
  // VkMemoryRequirements.alignment should be considered.
  vkBindBufferMemory(device, buffer, device_memory, /*memoryOffset=*/0);
  return device_memory;
}

// Returns the total data size to be copied.
VkDeviceSize GetTotalSize(absl::Span<const CopyInfo> infos) {
  VkDeviceSize total_size = 0;
  for (const auto& info : infos) {
    total_size += info.size;
  }
  return total_size;
}

// Maps device memory with the given 'map_offset' and 'map_size', and copies
// data from the host according to 'copy_infos'.
void CopyHostToBuffer(const Context& context,
                      const VkDeviceMemory& device_memory,
                      VkDeviceSize map_offset, VkDeviceSize map_size,
                      absl::Span<const CopyInfo> copy_infos) {
  // Data transfer may not happen immediately, for example, because it is only
  // written to cache and not yet to device. We can either flush host writes
  // with vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges, or
  // use VK_MEMORY_PROPERTY_HOST_COHERENT_BIT (a little less efficient).
  void* dst;
  vkMapMemory(*context.device(), device_memory, map_offset, map_size, nullflag,
              &dst);
  for (const auto& info : copy_infos) {
    std::memcpy(static_cast<char*>(dst) + info.offset, info.data, info.size);
  }
  vkUnmapMemory(*context.device(), device_memory);
}

}  // namespace

DeviceBuffer::AllocationInfo::AllocationInfo(
    const Context& context, UpdateRate update_rate,
    absl::Span<const BufferUsage> usages) {
  ASSERT_NON_EMPTY(usages, "Buffer has no usage");

  absl::flat_hash_set<uint32_t> queue_family_indices_set;
  for (const BufferUsage& usage : usages) {
    const auto queue_family_index = buffer::GetQueueFamilyIndex(context, usage);
    if (queue_family_index.has_value()) {
      queue_family_indices_set.insert(queue_family_index.value());
    }
  }

  // If the buffer is just used for data transfer, i.e. used as staging buffer,
  // we make it accessible from both graphics queue and compute queue for
  // simplicity.
  if (queue_family_indices_set.empty()) {
    const auto& queue_family_indices =
        context.physical_device().queue_family_indices();
    queue_family_indices_set = {
        queue_family_indices.graphics,
        queue_family_indices.compute,
    };
  }

  usage_flags = buffer::GetBufferUsageFlags(usages);
  switch (update_rate) {
    case UpdateRate::kLow:
      usage_flags |= buffer::GetBufferUsageFlags(
                         {BufferUsage::GetTransferDestinationUsage()});
      memory_property_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      break;

    case UpdateRate::kHigh:
      memory_property_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      memory_property_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      break;
  }
  unique_queue_family_indices = {queue_family_indices_set.begin(),
                                 queue_family_indices_set.end()};
}

void DeviceBuffer::AllocateBufferAndMemory(size_t size) {
  ASSERT_TRUE(size >= 0, "Buffer size must be non-negative");

  // Allocate only if the old buffer is not big enough.
  if (size <= buffer_size_) {
    return;
  }

  // Deallocate the old buffer if needed.
  DeallocateBufferAndMemory();

  buffer_size_ = size;
  buffer_ = CreateBuffer(*context_, buffer_size_, allocation_info_.usage_flags,
                         allocation_info_.unique_queue_family_indices);
  device_memory_ = CreateBufferMemory(*context_, buffer_,
                                      allocation_info_.memory_property_flags);
}

void DeviceBuffer::DeallocateBufferAndMemory() {
  if (buffer_size_ == 0) {
    return;
  }

  // Make copies of 'buffer_' and 'device_memory_' since they will be changed.
  const auto buffer = buffer_;
  const auto device_memory = device_memory_;
  context_->AddReleaseExpiredResourceOp(
      [buffer, device_memory](const Context& context) {
        DestroyBuffer(context, buffer);
        buffer::FreeDeviceMemory(context, device_memory);
      });

  buffer_size_ = 0;
  buffer_ = VK_NULL_HANDLE;
  device_memory_ = VK_NULL_HANDLE;
}

}  // namespace vk::renderer::lighter
