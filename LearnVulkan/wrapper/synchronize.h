//
//  synchronize.h
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef VULKAN_WRAPPER_SYNCHRONIZE_H
#define VULKAN_WRAPPER_SYNCHRONIZE_H

#include <vector>

#include <vulkan/vulkan.hpp>

namespace vulkan {
namespace wrapper {

/** VkSemaphore and VkFence are used for synchronization. Their constructions
 *      only requires VkDevice.
 */

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

} /* namespace wrapper */
} /* namespace vulkan */

#endif /* VULKAN_WRAPPER_SYNCHRONIZE_H */
