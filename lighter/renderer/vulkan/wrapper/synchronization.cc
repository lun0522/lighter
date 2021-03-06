//
//  synchronization.cc
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/synchronization.h"

#include "lighter/renderer/vulkan/wrapper/util.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

// Used to create a semaphore.
constexpr VkSemaphoreCreateInfo kSemaInfo{
    VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    /*pNext=*/nullptr,
    /*flags=*/nullflag,
};

// Used to create a fence which is initially signaled.
constexpr VkFenceCreateInfo kSignaledFenceInfo{
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    /*pNext=*/nullptr,
    /*flags=*/VK_FENCE_CREATE_SIGNALED_BIT,
};

// Used to create a fence which is initially unsignaled.
constexpr VkFenceCreateInfo kUnsignaledFenceInfo{
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    /*pNext=*/nullptr,
    /*flags=*/nullflag,
};

} /* namespace */

Semaphores::Semaphores(SharedBasicContext context, int count)
    : context_{std::move(FATAL_IF_NULL(context))}, semas_(count) {
  for (auto& sema : semas_) {
    ASSERT_SUCCESS(vkCreateSemaphore(*context_->device(), &kSemaInfo,
                                     *context_->allocator(), &sema),
                   "Failed to create semaphore");
  }
}

Semaphores::~Semaphores() {
  for (auto& sema : semas_) {
    vkDestroySemaphore(*context_->device(), sema, *context_->allocator());
  }
}

Fences::Fences(SharedBasicContext context, int count, bool is_signaled)
    : context_{std::move(FATAL_IF_NULL(context))}, fences_(count) {
  const VkFenceCreateInfo& fence_info =
      is_signaled ? kSignaledFenceInfo : kUnsignaledFenceInfo;
  for (auto& fence : fences_) {
    ASSERT_SUCCESS(vkCreateFence(*context_->device(), &fence_info,
                                 *context_->allocator(), &fence),
                   "Failed to create fence");
  }
}

Fences::~Fences() {
  for (auto& fence : fences_) {
    vkDestroyFence(*context_->device(), fence, *context_->allocator());
  }
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
