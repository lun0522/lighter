//
//  buffer.cc
//
//  Created by Pujun Lun on 10/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/buffer.h"

#include <vector>

#include "lighter/renderer/vk/buffer_util.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace {

// Creates a buffer of 'data_size'.
VkBuffer CreateBuffer(const Context& context, VkDeviceSize data_size,
                      VkBufferUsageFlags usage_flags,
                      VkSharingMode sharing_mode,
                      absl::Span<const uint32_t> unique_queue_family_indices) {
  const VkBufferCreateInfo buffer_info{
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      data_size,
      usage_flags,
      sharing_mode,
      CONTAINER_SIZE(unique_queue_family_indices),
      unique_queue_family_indices.data(),
  };

  VkBuffer buffer;
  ASSERT_SUCCESS(vkCreateBuffer(*context.device(), &buffer_info,
                                *context.host_allocator(), &buffer),
                 "Failed to create buffer");
  return buffer;
}

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

} /* namespace */

DeviceBuffer::AllocationInfo::AllocationInfo(
    const Context& context, UpdateRate update_rate,
    absl::Span<const BufferUsage> usages) {
  util::QueueUsage queue_usage;
  for (const BufferUsage& usage : usages) {
    const auto queue_family_index = buffer::GetQueueFamilyIndex(context, usage);
    if (queue_family_index.has_value()) {
      queue_usage.AddQueueFamily(queue_family_index.value());
    }
  }
  ASSERT_NON_EMPTY(queue_usage.unique_family_indices_set(),
                   "Cannot find any queue used for this buffer");

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
  sharing_mode = queue_usage.sharing_mode();
  unique_queue_family_indices = queue_usage.GetUniqueQueueFamilyIndices();
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
                         allocation_info_.sharing_mode,
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
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
