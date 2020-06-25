//
//  util.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/util.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace util {

QueueUsage::QueueUsage(std::vector<uint32_t>&& queue_family_indices) {
  ASSERT_NON_EMPTY(queue_family_indices, "Must contain at least one queue");
  common::util::RemoveDuplicate(&queue_family_indices);
  unique_family_indices_ = std::move(queue_family_indices);
  sharing_mode_ = unique_family_indices_.size() == 1
                      ? VK_SHARING_MODE_EXCLUSIVE
                      : VK_SHARING_MODE_CONCURRENT;
}

uint32_t FindMemoryTypeIndex(const VkPhysicalDevice& physical_device,
                             uint32_t memory_type,
                             VkMemoryPropertyFlags memory_properties) {
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);

  for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
    if ((1U << i) & memory_type) {
      if ((properties.memoryTypes[i].propertyFlags & memory_properties) ==
          memory_properties) {
        return i;
      }
    }
  }
  FATAL("Failed to find suitable memory type");
}

} /* namespace util */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
