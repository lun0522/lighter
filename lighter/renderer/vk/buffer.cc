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

using ir::BufferUsage;
using CopyInfo = Buffer::CopyInfo;

// Creates a buffer of 'data_size'.
intl::Buffer CreateBuffer(
    const Context& context, intl::DeviceSize data_size,
    intl::BufferUsageFlags usage_flags,
    const std::vector<uint32_t>& unique_queue_family_indices) {
  const auto buffer_create_info = intl::BufferCreateInfo{}
      .setSize(data_size)
      .setUsage(usage_flags)
      .setSharingMode(intl::SharingMode::eExclusive)
      .setQueueFamilyIndices(unique_queue_family_indices);
  return context.device()->createBuffer(buffer_create_info,
                                        *context.host_allocator());
}

// Allocates device memory for 'buffer' with 'memory_properties'.
intl::DeviceMemory CreateBufferMemory(const Context& context,
                                      intl::Buffer buffer,
                                      intl::MemoryPropertyFlags property_flags) {
  const intl::Device device = *context.device();
  intl::DeviceMemory device_memory = buffer::CreateDeviceMemory(
      context,  device.getBufferMemoryRequirements(buffer), property_flags);

  // Bind the allocated memory with 'buffer'. If this memory is used for
  // multiple buffers, the memory offset should be re-calculated and
  // VkMemoryRequirements.alignment should be considered.
  device.bindBufferMemory(buffer, device_memory, /*memoryOffset=*/0);
  return device_memory;
}

// Maps device memory with the given 'map_offset' and 'map_size', and copies
// data from the host according to 'copy_infos'.
void CopyHostToBuffer(const Context& context,
                      intl::DeviceMemory device_memory,
                      intl::DeviceSize map_offset, intl::DeviceSize map_size,
                      absl::Span<const CopyInfo> copy_infos) {
  // Data transfer may not happen immediately, for example, because it is only
  // written to cache and not yet to device. We can either flush host writes
  // with vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges, or
  // use VK_MEMORY_PROPERTY_HOST_COHERENT_BIT (a little less efficient).
  const intl::Device device = *context.device();
  void* dst = device.mapMemory(device_memory, map_offset, map_size,
                               /*flags=*/{});
  for (const auto& info : copy_infos) {
    std::memcpy(static_cast<char*>(dst) + info.offset, info.data, info.size);
  }
  device.unmapMemory(device_memory);
}

}  // namespace

Buffer::AllocationInfo::AllocationInfo(
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
      memory_property_flags |= intl::MemoryPropertyFlagBits::eDeviceLocal;
      break;

    case UpdateRate::kHigh:
      memory_property_flags |= intl::MemoryPropertyFlagBits::eHostVisible;
      memory_property_flags |= intl::MemoryPropertyFlagBits::eHostCoherent;
      break;
  }
  unique_queue_family_indices = {queue_family_indices_set.begin(),
                                 queue_family_indices_set.end()};
}

void Buffer::AllocateBufferAndMemory(size_t size) {
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

void Buffer::DeallocateBufferAndMemory() {
  if (buffer_size_ == 0) {
    return;
  }

  // Make copies of 'buffer_' and 'device_memory_' since they will be changed.
  const auto buffer = buffer_;
  const auto device_memory = device_memory_;
  context_->AddReleaseExpiredResourceOp(
      [buffer, device_memory](const Context& context) {
        context.DeviceDestroy(buffer);
        buffer::FreeDeviceMemory(context, device_memory);
      });

  buffer_size_ = 0;
  buffer_ = nullptr;
  device_memory_ = nullptr;
}

void Buffer::CopyToDevice(absl::Span<const CopyInfo> copy_infos) const {
  if (allocation_info_.IsHostVisible()) {
    CopyHostToBuffer(*context_, device_memory_, /*map_offset=*/0, buffer_size_,
                     copy_infos);
  } else {
    FATAL("Not implemented yet");
  }
}

}  // namespace lighter::renderer::vk
