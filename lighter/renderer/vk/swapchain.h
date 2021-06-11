//
//  swapchain.h
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_SWAPCHAIN_H
#define LIGHTER_RENDERER_VK_SWAPCHAIN_H

#include <memory>
#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/basic.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter::renderer::vk {

// Wraps VkSwapchainKHR, which holds a queue of images to present to the screen.
class Swapchain {
 public:
  Swapchain(SharedContext context, int window_index,
            const common::Window& window);

  // This class is neither copyable nor movable.
  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  ~Swapchain();

  // Returns the names of required Vulkan extensions for the swapchain.
  static const std::vector<const char*>& GetRequiredExtensions();

  // Accessors.
  const SwapchainImage& image() const { return *image_; }

 private:
  // Pointer to context.
  const SharedContext context_;

  // Opaque swapchain object,
  VkSwapchainKHR swapchain_;

  // Wraps images retrieved from the swapchain.
  std::unique_ptr<SwapchainImage> image_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_BASIC_H
