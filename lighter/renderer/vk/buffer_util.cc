//
//  buffer_util.cc
//
//  Created by Pujun Lun on 11/3/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/buffer_util.h"

#include "lighter/renderer/type.h"

namespace lighter::renderer::vk::buffer {
namespace {

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
          FATAL("Access type must not be kDontCare or kReadWrite for usage "
                "type kTransfer");

        case AccessType::kReadOnly:
          return UsageFlag::eTransferSrc;

        case AccessType::kWriteOnly:
          return UsageFlag::eTransferDst;
      }
  }
}

// Returns the index of a VkMemoryType that satisfies both 'memory_type' and
// 'property_flags' within VkPhysicalDeviceMemoryProperties.memoryTypes.
uint32_t FindMemoryTypeIndex(intl::PhysicalDevice physical_device,
                             uint32_t memory_type,
                             intl::MemoryPropertyFlags property_flags) {
  const auto properties = physical_device.getMemoryProperties();
  for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
    if ((1U << i) & memory_type) {
      if ((properties.memoryTypes[i].propertyFlags & property_flags) ==
          property_flags) {
        return i;
      }
    }
  }
  FATAL("Failed to find suitable device memory");
}

}  // namespace

intl::BufferUsageFlags GetBufferUsageFlags(
    absl::Span<const BufferUsage> usages) {
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
          FATAL("Access location must not be kDontCare, kHost or kOther "
                "for usage type kUniform");

        case AccessLocation::kVertexShader:
        case AccessLocation::kFragmentShader:
          return queue_family_indices.graphics;

        case AccessLocation::kComputeShader:
          return queue_family_indices.compute;
      }
  }
}

intl::DeviceMemory CreateDeviceMemory(
    const Context& context, const intl::MemoryRequirements& requirements,
    intl::MemoryPropertyFlags property_flags) {
  const auto memory_create_info = intl::MemoryAllocateInfo{}
      .setAllocationSize(requirements.size)
      .setMemoryTypeIndex(FindMemoryTypeIndex(*context.physical_device(),
                              requirements.memoryTypeBits, property_flags));
  return context.device()->allocateMemory(memory_create_info,
                                          *context.host_allocator());
}

}  // namespace lighter::renderer::vk::buffer
