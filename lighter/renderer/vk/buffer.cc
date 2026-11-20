//
//  buffer.cc
//
//  Created by Pujun Lun on 10/25/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/buffer.h"

#include <algorithm>
#include <array>
#include <format>
#include <unordered_set>
#include <vector>

#include "lighter/renderer/vk/buffer_util.h"

namespace lighter::renderer::vk {
namespace {

using ir::BufferUsage;
using CopyInfo = Buffer::CopyInfo;

}  // namespace

Buffer::AllocationInfo::AllocationInfo(const Context& context,
                                       UpdateRate update_rate,
                                       std::span<const BufferUsage> usages) {
  ASSERT_NON_EMPTY(usages, "Buffer has no usage");

  std::unordered_set<uint32_t> queue_family_indices_set;
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
          std::array{BufferUsage::GetTransferDestinationUsage()});
      queue_family_indices_set.insert(
          context.physical_device().queue_family_indices().graphics);
      host_visible = false;
      break;

    case UpdateRate::kHigh:
      host_visible = true;
      break;
  }
  unique_queue_family_indices = {queue_family_indices_set.begin(),
                                 queue_family_indices_set.end()};
}

void Buffer::AllocateBufferAndMemory(size_t size) {
  // Allocate only if the old buffer is not big enough.
  if (size <= buffer_size_) {
    return;
  }

  // Deallocate the old buffer if needed.
  DeallocateBufferAndMemory();

  buffer_size_ = size;
  const buffer::AllocatedBuffer allocated = buffer::CreateBuffer(
      *context_, buffer_size_, allocation_info_.usage_flags,
      allocation_info_.unique_queue_family_indices,
      allocation_info_.host_visible);
  buffer_ = allocated.handle;
  allocation_ = allocated.allocation;
}

void Buffer::DeallocateBufferAndMemory() {
  if (buffer_size_ == 0) {
    return;
  }

  // Make copies of 'buffer_' and 'allocation_' since they will be changed.
  const buffer::AllocatedBuffer expired{buffer_, allocation_};
  context_->AddReleaseExpiredResourceOp([expired](const Context& context) {
    buffer::DestroyBuffer(context, expired);
  });

  buffer_size_ = 0;
  buffer_ = nullptr;
  allocation_ = VK_NULL_HANDLE;
}

void Buffer::CopyToDevice(std::span<const CopyInfo> copy_infos) const {
  if (copy_infos.empty()) {
    return;
  }
  for (const auto& info : copy_infos) {
    ASSERT_TRUE(
        info.offset + info.size <= buffer_size_,
        std::format("Copy range [{}, {}) exceeds buffer size {}", info.offset,
                    info.offset + info.size, buffer_size_));
  }

  if (allocation_info_.host_visible) {
    buffer::CopyHostDataToAllocation(*context_, allocation_, copy_infos);
    return;
  }

  // Pack all copy regions contiguously into a transient staging buffer.
  size_t total_size = 0;
  for (const auto& info : copy_infos) {
    total_size += info.size;
  }
  if (total_size == 0) {
    return;
  }

  std::vector<CopyInfo> staging_copies;
  std::vector<intl::BufferCopy> copy_regions;
  staging_copies.reserve(copy_infos.size());
  copy_regions.reserve(copy_infos.size());
  size_t staging_offset = 0;
  for (const auto& info : copy_infos) {
    if (info.size == 0) {
      continue;
    }
    staging_copies.push_back(CopyInfo{info.data, info.size, staging_offset});
    copy_regions.push_back(intl::BufferCopy{}
                               .setSrcOffset(staging_offset)
                               .setDstOffset(info.offset)
                               .setSize(info.size));
    staging_offset += info.size;
  }

  const buffer::AllocatedBuffer staging =
      buffer::CreateStagingBuffer(*context_, total_size);
  buffer::CopyHostDataToAllocation(*context_, staging.allocation,
                                   staging_copies);
  context_->RunOneTimeGraphicsCommand([&](intl::CommandBuffer cmd) {
    cmd.copyBuffer(staging.handle, buffer_, copy_regions);
  });
  buffer::DestroyBuffer(*context_, staging);
}

}  // namespace lighter::renderer::vk
