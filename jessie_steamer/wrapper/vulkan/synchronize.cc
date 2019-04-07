//
//  synchronize.cc
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/synchronize.h"

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/context.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

constexpr VkSemaphoreCreateInfo kSemaInfo{
    VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    /*pNext=*/nullptr,
    util::nullflag,
};

constexpr VkFenceCreateInfo kSignaledFenceInfo{
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    /*pNext=*/nullptr,
    /*flags=*/VK_FENCE_CREATE_SIGNALED_BIT,
};

constexpr VkFenceCreateInfo kUnsignaledFenceInfo{
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    /*pNext=*/nullptr,
    util::nullflag,
};

} /* namespace */

void Semaphores::Init(SharedContext context, size_t count) {
  context_ = std::move(context);
  semas_.resize(count);
  for (auto& sema : semas_) {
    ASSERT_SUCCESS(vkCreateSemaphore(*context_->device(), &kSemaInfo,
                                     context_->allocator(), &sema),
                   "Failed to create semaphore");
  }
}

Semaphores::~Semaphores() {
  for (auto& sema : semas_) {
    vkDestroySemaphore(*context_->device(), sema, context_->allocator());
  }
}

void Fences::Init(SharedContext context,
                  size_t count,
                  bool is_signaled) {
  context_ = std::move(context);
  fences_.resize(count);
  const VkFenceCreateInfo& fence_info =
      is_signaled ? kSignaledFenceInfo : kUnsignaledFenceInfo;
  for (auto& fence : fences_) {
    ASSERT_SUCCESS(vkCreateFence(*context_->device(), &fence_info,
                                 context_->allocator(), &fence),
                   "Failed to create fence");
  }
}

Fences::~Fences() {
  for (auto& fence : fences_) {
    vkDestroyFence(*context_->device(), fence, context_->allocator());
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
