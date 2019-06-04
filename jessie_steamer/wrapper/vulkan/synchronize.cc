//
//  synchronize.cc
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/synchronize.h"

#include "jessie_steamer/wrapper/vulkan/macro.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

constexpr VkSemaphoreCreateInfo kSemaInfo{
    VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    /*pNext=*/nullptr,
    /*flags=*/nullflag,
};

constexpr VkFenceCreateInfo kSignaledFenceInfo{
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    /*pNext=*/nullptr,
    /*flags=*/VK_FENCE_CREATE_SIGNALED_BIT,
};

constexpr VkFenceCreateInfo kUnsignaledFenceInfo{
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    /*pNext=*/nullptr,
    /*flags=*/nullflag,
};

} /* namespace */

void Semaphores::Init(const VkDevice* device,
                      const VkAllocationCallbacks* allocator,
                      int count) {
  device_ = device;
  allocator_ = allocator;
  semas_.resize(static_cast<size_t>(count));
  for (auto& sema : semas_) {
    ASSERT_SUCCESS(vkCreateSemaphore(*device_, &kSemaInfo, allocator_, &sema),
                   "Failed to create semaphore");
  }
}

Semaphores::~Semaphores() {
  for (auto& sema : semas_) {
    vkDestroySemaphore(*device_, sema, allocator_);
  }
}

void Fences::Init(const VkDevice* device,
                  const VkAllocationCallbacks* allocator,
                  int count, bool is_signaled) {
  device_ = device;
  allocator_ = allocator;
  fences_.resize(static_cast<size_t>(count));
  const VkFenceCreateInfo& fence_info =
      is_signaled ? kSignaledFenceInfo : kUnsignaledFenceInfo;
  for (auto& fence : fences_) {
    ASSERT_SUCCESS(vkCreateFence(*device_, &fence_info, allocator_, &fence),
                   "Failed to create fence");
  }
}

Fences::~Fences() {
  for (auto& fence : fences_) {
    vkDestroyFence(*device_, fence, allocator_);
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
