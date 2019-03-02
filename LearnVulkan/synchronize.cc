//
//  synchronize.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "synchronize.h"

#include "util.h"

using std::vector;

namespace vulkan {

namespace {

const VkSemaphoreCreateInfo kSemaInfo {
  .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
};

const VkFenceCreateInfo kFenceInfo {
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
  if (!is_signaled) {
    ASSERT_SUCCESS(vkCreateFence(device, &kFenceInfo, nullptr, &fence),
                   "Failed to create fence");
  } else {
    VkFenceCreateInfo fence_info = kFenceInfo;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    ASSERT_SUCCESS(vkCreateFence(device, &fence_info, nullptr, &fence),
                   "Failed to create fence");
  }
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

} /* namespace vulkan */
