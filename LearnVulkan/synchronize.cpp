//
//  synchronize.cpp
//  LearnVulkan
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "synchronize.hpp"

#include "util.h"

using std::vector;

namespace vulkan {

namespace {

VkSemaphoreCreateInfo kSemaInfo {
  .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
};

VkFenceCreateInfo kFenceInfo {
  .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
  .flags = VK_FENCE_CREATE_SIGNALED_BIT,
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

VkFence CreateFence(const VkDevice& device) {
  VkFence fence{};
  ASSERT_SUCCESS(vkCreateFence(device, &kFenceInfo, nullptr, &fence),
                 "Failed to create fence");
  return fence;
}

vector<VkFence> CreateFences(size_t count,
                             const VkDevice& device) {
  vector<VkFence> fences(count);
  for (auto& fence : fences)
    fence = CreateFence(device);
  return fences;
}

} /* namespace vulkan */
