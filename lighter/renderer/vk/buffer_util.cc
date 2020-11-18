//
//  buffer_util.cc
//
//  Created by Pujun Lun on 11/3/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/buffer_util.h"

#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/util.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace buffer {
namespace {

using UsageType = BufferUsage::UsageType;

// Returns VkBufferUsageFlags for 'usage'. Note that this must not be called
// if usage type is kDontCare, since it doesn't have corresponding flag bits.
VkBufferUsageFlags GetBufferUsageFlags(const BufferUsage& usage) {
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
      FATAL("No corresponding buffer usage flags for usage type kDontCare");

    case UsageType::kIndexOnly:
      return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    case UsageType::kVertexOnly:
      return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    case UsageType::kIndexAndVertex:
      return VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                 | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    case UsageType::kUniform:
      return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    case UsageType::kTransfer:
      switch (usage.access_type()) {
        case AccessType::kDontCare:
        case AccessType::kReadWrite:
          FATAL("Access type must not be kDontCare or kReadWrite for usage "
                "type kTransfer");

        case AccessType::kReadOnly:
          return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        case AccessType::kWriteOnly:
          return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      }
  }
}

// Returns the index of a VkMemoryType that satisfies both 'memory_type' and
// 'property_flags' within VkPhysicalDeviceMemoryProperties.memoryTypes.
uint32_t FindMemoryTypeIndex(const VkPhysicalDevice& physical_device,
                             uint32_t memory_type,
                             VkMemoryPropertyFlags property_flags) {
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);

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

} /* namespace */

VkBufferUsageFlags GetBufferUsageFlags(absl::Span<const BufferUsage> usages) {
  auto flags = nullflag;
  for (const auto& usage : usages) {
    if (usage.usage_type() != UsageType::kDontCare) {
      flags |= GetBufferUsageFlags(usage);
    }
  }
  return static_cast<VkImageUsageFlags>(flags);
}

absl::optional<uint32_t> GetQueueFamilyIndex(const Context& context,
                                             const BufferUsage& usage) {
  const auto& queue_family_indices =
      context.physical_device().queue_family_indices();
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
    case UsageType::kTransfer:
      return absl::nullopt;

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

VkDeviceMemory CreateDeviceMemory(
    const Context& context, const VkMemoryRequirements& requirements,
    VkMemoryPropertyFlags property_flags) {
  const VkMemoryAllocateInfo memory_info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      /*allocationSize=*/requirements.size,
      FindMemoryTypeIndex(*context.physical_device(),
                          requirements.memoryTypeBits, property_flags),
  };

  VkDeviceMemory memory;
  ASSERT_SUCCESS(vkAllocateMemory(*context.device(), &memory_info,
                                  *context.host_allocator(), &memory),
                 "Failed to allocate device memory");
  return memory;
}

} /* namespace buffer */
} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
