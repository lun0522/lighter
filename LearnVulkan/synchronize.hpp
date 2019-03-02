//
//  synchronize.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef synchronize_hpp
#define synchronize_hpp

#include <vector>

#include <vulkan/vulkan.hpp>

namespace vulkan {

VkSemaphore CreateSemaphore(const VkDevice& device);

std::vector<VkSemaphore> CreateSemaphores(
    size_t count,
    const VkDevice& device);

VkFence CreateFence(const VkDevice& device);

std::vector<VkFence> CreateFences(
    size_t count,
    const VkDevice& device);

} /* namespace vulkan */

#endif /* synchronize_hpp */
