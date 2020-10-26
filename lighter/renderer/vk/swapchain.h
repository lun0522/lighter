//
//  swapchain.h
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_SWAPCHAIN_H
#define LIGHTER_RENDERER_VK_SWAPCHAIN_H

#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/basic.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {

// Wraps VkSwapchainKHR, which holds a queue of images to present to the screen.
class Swapchain {
 public:
  Swapchain(SharedContext context, int window_index,
            const common::Window& window, MultisamplingMode multisampling_mode);

  // This class is neither copyable nor movable.
  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  ~Swapchain();

  // Returns the names of required Vulkan extensions for the swapchain.
  static const std::vector<const char*>& GetRequiredExtensions();

 private:
  // Pointer to context.
  const SharedContext context_;

  // Opaque swapchain object,
  VkSwapchainKHR swapchain_;

  // Wraps images retrieved from the swapchain.
  std::vector<std::unique_ptr<DeviceImage>> swapchain_images_;

  // This should have value if multisampling is requested. We only need one
  // instance of it since we only render to one frame at any time.
  std::unique_ptr<DeviceImage> multisample_image_;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_BASIC_H */
