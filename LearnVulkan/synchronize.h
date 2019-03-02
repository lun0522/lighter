//
//  synchronize.h
//  LearnVulkan
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef SYNCHRONIZE_H
#define SYNCHRONIZE_H

#include <vector>

#include <vulkan/vulkan.hpp>

namespace vulkan {

VkSemaphore CreateSemaphore(
    const VkDevice& device);

std::vector<VkSemaphore> CreateSemaphores(
    size_t count,
    const VkDevice& device);

VkFence CreateFence(
    const VkDevice& device,
    bool is_signaled = false);

std::vector<VkFence> CreateFences(
    size_t count,
    const VkDevice& device,
    bool is_signaled = false);

} /* namespace vulkan */

#endif /* SYNCHRONIZE_H */
