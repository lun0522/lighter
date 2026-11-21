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

#include "lighter/base/util.h"
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
            const base::Window& window);

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
  intl::Extent2D extent() const { return extent_; }
  intl::Format format() const { return format_; }
  uint32_t num_images() const { return CAST_TO_UINT(images_.size()); }
  const Image& frame_image(uint32_t image_index) const {
    return *frame_images_.at(image_index);
  }
  intl::Image vk_image(uint32_t image_index) const {
    return images_.at(image_index);
  }
  intl::ImageView image_view(uint32_t image_index) const {
    return image_views_.at(image_index);
  }
  intl::Semaphore render_finished_semaphore(uint32_t image_index) const {
    return render_finished_semaphores_.at(image_index);
  }

 private:
  const int window_index_;
  intl::Format format_ = intl::Format::eUndefined;
  intl::Extent2D extent_;

  // Opaque swapchain object.
  intl::SwapchainKHR swapchain_;

  // Swapchain-owned images, image views, and per-index vk::Image wrappers.
  std::vector<intl::Image> images_;
  std::vector<intl::ImageView> image_views_;
  std::vector<std::unique_ptr<Image>> frame_images_;

  // Signaled when rendering completes for each swapchain image.
  std::vector<intl::Semaphore> render_finished_semaphores_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_SWAPCHAIN_H
