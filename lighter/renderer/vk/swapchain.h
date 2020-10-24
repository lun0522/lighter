//
//  swapchain.h
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_SWAPCHAIN_H
#define LIGHTER_RENDERER_VK_SWAPCHAIN_H

#include <vector>

#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {

// Wraps VkSwapchainKHR, which holds a queue of images to present to the screen.
class Swapchain {
 public:
  // Returns the names of required Vulkan extensions for the swapchain.
  static const std::vector<const char*>& GetRequiredExtensions();
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_BASIC_H */
