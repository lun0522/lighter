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

VkSemaphore CreateSemaphore(const VkDevice& device) {
  VkSemaphore sema{};
  ASSERT_SUCCESS(vkCreateSemaphore(device, &kSemaInfo, nullptr, &sema),
                 "Failed to create semaphore");
  return sema;
}

vector<VkSemaphore> CreateSemaphores(size_t count,
                                     const VkDevice& device) {
  vector<VkSemaphore> semas(count);
  for (auto& sema : semas)
    sema = CreateSemaphore(device);
  return semas;
}

VkFence CreateFence(const VkDevice& device, bool is_signaled) {
  VkFence fence{};
  const VkFenceCreateInfo& fence_info =
      is_signaled ? kSignaledFenceInfo : kUnsignaledFenceInfo;
  ASSERT_SUCCESS(vkCreateFence(device, &fence_info, nullptr, &fence),
                 "Failed to create fence");
  return fence;
}

vector<VkFence> CreateFences(size_t count,
                             const VkDevice& device,
                             bool is_signaled) {
  vector<VkFence> fences(count);
  for (auto& fence : fences)
    fence = CreateFence(device, is_signaled);
  return fences;
}

} /* namespace wrapper */
} /* namespace vulkan */
