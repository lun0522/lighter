//
//  synchronize.cc
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "synchronize.h"

#include "util.h"

using std::vector;

namespace vulkan {
namespace wrapper {
namespace {

constexpr VkSemaphoreCreateInfo kSemaInfo{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
};

constexpr VkFenceCreateInfo kSignaledFenceInfo{
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
};

constexpr VkFenceCreateInfo kUnsignaledFenceInfo{
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
};

} /* namespace */

vector<VkSemaphore> CreateSemaphores(size_t count,
                                     const VkDevice& device) {
  vector<VkSemaphore> semas(count);
  for (auto& sema : semas) {
    ASSERT_SUCCESS(vkCreateSemaphore(device, &kSemaInfo, nullptr, &sema),
                   "Failed to create semaphore");
  }
  return semas;
}

vector<VkFence> CreateFences(size_t count,
                             const VkDevice& device,
                             bool is_signaled) {
  const VkFenceCreateInfo& fence_info =
      is_signaled ? kSignaledFenceInfo : kUnsignaledFenceInfo;
  vector<VkFence> fences(count);
  for (auto& fence : fences) {
    ASSERT_SUCCESS(vkCreateFence(device, &fence_info, nullptr, &fence),
                   "Failed to create fence");
  }
  return fences;
}

} /* namespace wrapper */
} /* namespace vulkan */
