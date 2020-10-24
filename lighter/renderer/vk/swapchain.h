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
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {

// Holds objects commonly used for onscreen rendering apart from the basic ones.
class WindowContext {
 public:
  WindowContext(const common::Window* window, const Surface* surface,
                int window_index)
      : window_{*FATAL_IF_NULL(window)}, surface_{*FATAL_IF_NULL(surface)},
        window_index_{window_index} {}

  // This class is neither copyable nor movable.
  WindowContext(const WindowContext&) = delete;
  WindowContext& operator=(const WindowContext&) = delete;

  // Accessors.
  const common::Window& window() const { return window_; }
  const Surface& surface() const { return surface_; }
  int window_index() const { return window_index_; }

 private:
  // Window created by user.
  const common::Window& window_;

  // Wrapper of VkSurfaceKHR.
  const Surface& surface_;

  // Index of this window among all windows created by user.
  const int window_index_;
};

// Wraps VkSwapchainKHR, which holds a queue of images to present to the screen.
class Swapchain {
 public:
  Swapchain(SharedContext context, const WindowContext& window_context,
            MultisamplingMode multisampling_mode);

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

  // Extent of each swapchain image.
  VkExtent2D image_extent_;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_BASIC_H */
