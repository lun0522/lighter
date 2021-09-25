//
//  swapchain.h
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_SWAPCHAIN_H
#define LIGHTER_RENDERER_VK_SWAPCHAIN_H

#include <memory>

#include "lighter/common/util.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/basic.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

// Wraps VkSwapchainKHR, which holds a queue of images to present to the screen.
class Swapchain : WithSharedContext {
 public:
  Swapchain(const SharedContext& context, int window_index,
            const common::Window& window);

  // This class is neither copyable nor movable.
  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  ~Swapchain();

  // Accessors.
  const MultiImage& image() const { return *image_; }

 private:
  // Opaque swapchain object,
  intl::SwapchainKHR swapchain_;

  // Wraps images retrieved from the swapchain.
  std::unique_ptr<MultiImage> image_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_BASIC_H
