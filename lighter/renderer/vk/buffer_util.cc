//
//  buffer_util.cc
//
//  Created by Pujun Lun on 11/3/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/buffer_util.h"

#include <cstring>

#include "lighter/renderer/ir/type.h"

namespace lighter::renderer::vk::buffer {
namespace {

using ir::AccessLocation;
using ir::AccessType;
using ir::BufferUsage;
using UsageType = BufferUsage::UsageType;

// Returns VkBufferUsageFlags for 'usage'. Note that this must not be called
// if usage type is kDontCare, since it doesn't have corresponding flag bits.
intl::BufferUsageFlags GetBufferUsageFlags(const BufferUsage& usage) {
  using UsageFlag = intl::BufferUsageFlagBits;
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
      FATAL("No corresponding buffer usage flags for usage type kDontCare");

    case UsageType::kIndexOnly:
      return UsageFlag::eIndexBuffer;

    case UsageType::kVertexOnly:
      return UsageFlag::eVertexBuffer;

    case UsageType::kIndexAndVertex:
      return UsageFlag::eIndexBuffer | UsageFlag::eVertexBuffer;

    case UsageType::kUniform:
      return UsageFlag::eUniformBuffer;

    case UsageType::kTransfer:
      switch (usage.access_type()) {
        case AccessType::kDontCare:
        case AccessType::kReadWrite:
          FATAL(
              "Access type must not be kDontCare or kReadWrite for usage "
              "type kTransfer");

        case AccessType::kReadOnly:
          return UsageFlag::eTransferSrc;

        case AccessType::kWriteOnly:
          return UsageFlag::eTransferDst;
      }
  }
}

}  // namespace

intl::BufferUsageFlags GetBufferUsageFlags(
    std::span<const BufferUsage> usages) {
  intl::BufferUsageFlags flags;
  for (const auto& usage : usages) {
    if (usage.usage_type() != UsageType::kDontCare) {
      flags |= GetBufferUsageFlags(usage);
    }
  }
  return flags;
}

std::optional<uint32_t> GetQueueFamilyIndex(const Context& context,
                                            const BufferUsage& usage) {
  const auto& queue_family_indices =
      context.physical_device().queue_family_indices();
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
    case UsageType::kTransfer:
      return std::nullopt;

    case UsageType::kIndexOnly:
    case UsageType::kVertexOnly:
    case UsageType::kIndexAndVertex:
      return queue_family_indices.graphics;

    case UsageType::kUniform:
      switch (usage.access_location()) {
        case AccessLocation::kDontCare:
        case AccessLocation::kHost:
        case AccessLocation::kOther:
          FATAL(
              "Access location must not be kDontCare, kHost or kOther "
              "for usage type kUniform");

        case AccessLocation::kVertexShader:
        case AccessLocation::kFragmentShader:
          return queue_family_indices.graphics;

        case AccessLocation::kComputeShader:
          return queue_family_indices.compute;
      }
  }
}

AllocatedBuffer CreateBuffer(
    const Context& context, intl::DeviceSize size,
    intl::BufferUsageFlags usage_flags,
    const std::vector<uint32_t>& unique_queue_family_indices,
    bool host_visible) {
  const auto buffer_create_info =
      intl::BufferCreateInfo{}
          .setSize(size)
          .setUsage(usage_flags)
          .setSharingMode(intl::SharingMode::eExclusive)
          .setQueueFamilyIndices(unique_queue_family_indices);
  const auto& c_buffer_info =
      static_cast<const VkBufferCreateInfo&>(buffer_create_info);

  VmaAllocationCreateInfo alloc_create_info{};
  if (host_visible) {
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_create_info.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
  } else {
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  }

  VkBuffer raw_buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  ASSERT_SUCCESS(intl::Result{vmaCreateBuffer(
                     context.vma_allocator(), &c_buffer_info,
                     &alloc_create_info, &raw_buffer, &allocation, nullptr)},
                 "Failed to create VMA buffer");
  return AllocatedBuffer{intl::Buffer{raw_buffer}, allocation};
}

AllocatedBuffer CreateStagingBuffer(const Context& context,
                                    intl::DeviceSize size) {
  const std::vector<uint32_t> queue_family_indices{
      context.physical_device().queue_family_indices().graphics};
  return CreateBuffer(context, size, intl::BufferUsageFlagBits::eTransferSrc,
                      queue_family_indices, /*host_visible=*/true);
}

void DestroyBuffer(const Context& context, const AllocatedBuffer& buffer) {
  if (buffer.allocation != VK_NULL_HANDLE) {
    vmaDestroyBuffer(context.vma_allocator(),
                     static_cast<VkBuffer>(buffer.handle), buffer.allocation);
  }
}

void CopyHostDataToAllocation(
    const Context& context, VmaAllocation allocation,
    std::span<const ir::Buffer::CopyInfo> copy_infos) {
  VmaAllocationInfo alloc_info{};
  vmaGetAllocationInfo(context.vma_allocator(), allocation, &alloc_info);
  ASSERT_NON_NULL(alloc_info.pMappedData, "VMA allocation is not mapped");
  for (const auto& info : copy_infos) {
    if (info.size > 0) {
      std::memcpy(static_cast<char*>(alloc_info.pMappedData) + info.offset,
                  info.data, info.size);
    }
  }
  ASSERT_SUCCESS(
      intl::Result{vmaFlushAllocation(context.vma_allocator(), allocation,
                                      /*offset=*/0, VK_WHOLE_SIZE)},
      "Failed to flush VMA allocation");
}

}  // namespace lighter::renderer::vk::buffer
