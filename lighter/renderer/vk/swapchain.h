//
//  swapchain.h
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_SWAPCHAIN_H
#define LIGHTER_RENDERER_VK_SWAPCHAIN_H

#include <cstdint>
#include <memory>
#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/basic.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

// Wraps VkSwapchainKHR, which holds a queue of images to present to the screen.
class Swapchain : public WithSharedContext {
 public:
  Swapchain(const SharedContext& context, int window_index,
            const common::Window& window);

  // This class is neither copyable nor movable.
  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  ~Swapchain() override;

  // Acquires the next presentable image index and signals 'signal_semaphore'.
  intl::Result AcquireNextImage(intl::Semaphore signal_semaphore,
                                uint32_t* out_image_index) const;

  // Presents 'image_index' after waiting on 'wait_semaphore'.
  intl::Result Present(uint32_t image_index,
                       intl::Semaphore wait_semaphore) const;

  // Accessors.
  const MultiImage& image() const { return *image_; }
  intl::Semaphore render_finished_semaphore(uint32_t image_index) const {
    return render_finished_semaphores_.at(image_index);
  }

 private:
  const int window_index_;

  // Opaque swapchain object.
  intl::SwapchainKHR swapchain_;

  // Wraps images retrieved from the swapchain.
  std::unique_ptr<MultiImage> image_;

  // Signaled when rendering completes for each swapchain image.
  std::vector<intl::Semaphore> render_finished_semaphores_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_SWAPCHAIN_H
