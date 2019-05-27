//
//  swapchain.h
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_SWAPCHAIN_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_SWAPCHAIN_H

#include <memory>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/image.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

/** VkSwapchainKHR holds a queue of images to present to the screen.
 *
 *  Initialization:
 *    VkPhysicalDevice (query image extent and format, and present mode)
 *    VkDevice
 *    VkSurfaceKHR
 *    How many images it should hold at least
 *    Surface format of images (R5G6B5, R8G8B8, R8G8B8A8, etc)
 *    Color space of images (sRGB, etc)
 *    Extent of images
 *    Number of layers in each image (maybe useful for stereoscopic apps)
 *    Usage of images (color attachment, depth stencil, etc)
 *    Sharing mode (whether images are shared by multiple queue families.
 *      if shared, we have to specify how many families will share, and
 *      the index of each family)
 *    What pre-transform to do (rotate or mirror images)
 *    What alpha composition to do
 *    Present mode (immediate, mailbox, fifo, etc)
 *    Whether to ignore the color of pixels that are obscured
 *    Old swapchain (when we recreate the swapchain, we don't have to
 *      wait until the old one finishes all operations, but go ahead to
 *      create a new one and inform it of the old one, so that the
 *      transition is more seamless)
 */
class Swapchain {
 public:
  static const std::vector<const char*>& extensions();
  static bool HasSwapchainSupport(const std::shared_ptr<Context>& context,
                                  const VkPhysicalDevice& physical_device);

  Swapchain() = default;

  // This class is neither copyable nor movable
  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  ~Swapchain() { Cleanup(); }

  void Init(const std::shared_ptr<Context>& context);
  void Cleanup();

  const VkSwapchainKHR& operator*() const { return swapchain_; }
  VkFormat format()                 const { return image_format_; }
  VkExtent2D extent()               const { return image_extent_; }
  size_t size()                     const { return images_.size(); }
  const VkImageView& image_view(int index) const
      { return images_[index]->image_view(); }

 private:
  std::shared_ptr<Context> context_;
  VkSwapchainKHR swapchain_;
  std::vector<std::unique_ptr<SwapChainImage>> images_;
  VkFormat image_format_;
  VkExtent2D image_extent_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_SWAPCHAIN_H */
