//
//  swapchain.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_SWAPCHAIN_H
#define LEARNVULKAN_SWAPCHAIN_H

#include <vector>

#include <vulkan/vulkan.hpp>

#include "util.h"

namespace vulkan {

class Application;

/** VkSwapchainKHR holds a queue of images to present to the screen.
 *
 *  Initialization:
 *      VkPhysicalDevice (query image extent and format, and present mode)
 *      VkDevice
 *      VkSurfaceKHR
 *      How many images it should hold at least
 *      Surface format of images (R5G6B5, R8G8B8, R8G8B8A8, etc)
 *      Color space of images (sRGB, etc)
 *      Extent of images
 *      Number of layers in each image (maybe useful for stereoscopic apps)
 *      Usage of images (color attachment, depth stencil, etc)
 *      Sharing mode (whether images are shared by multiple queue families.
 *          if shared, we have to specify how many families will share, and
 *          the index of each family)
 *      What pre-transform to do (rotate or mirror images)
 *      What alpha composition to do
 *      Present mode (immediate, mailbox, fifo, etc)
 *      Whether to ignore the color of pixels that are obscured
 *      Old swap chain (when we recreate the swap chain, we don't have to
 *          wait until the old one finishes all operations, but go ahead to
 *          create a new one and inform it of the old one, so that the
 *          transition is more seamless)
 *
 *--------------------------------------------------------------------------
 *
 *  VkImage represents multidimensional data in the swap chain. They can be
 *      color/depth/stencil attachements, textures, etc. The exact purpose
 *      is not specified until we create an image view.
 *
 *  Initialization:
 *      VkDevice
 *      VkSwapchainKHR
 *
 *--------------------------------------------------------------------------
 *
 *  VkImageView determines how to access and what part of images to access.
 *      We might convert the image format on the fly with it.
 *
 *  Initialization:
 *      VkDevice
 *      Image referenced by it
 *      View type (1D, 2D, 3D, cube, etc.)
 *      Format of the image
 *      Whether and how to remap RGBA channels
 *      Purpose of the image (color, depth, stencil, etc)
 *      Set of mipmap levels and array layers to be accessible
 */
class Swapchain {
 public:
  static bool HasSwapchainSupport(const VkSurfaceKHR& surface,
                                  const VkPhysicalDevice& physical_device);

  Swapchain(const Application& app) : app_{app} {}
  void Init();
  void Cleanup();
  ~Swapchain() { Cleanup(); }
  MARK_NOT_COPYABLE_OR_MOVABLE(Swapchain);

  const VkSwapchainKHR& operator*(void)         const { return swapchain_; }
  VkFormat format()                             const { return image_format_; }
  VkExtent2D extent()                           const { return image_extent_; }
  const std::vector<VkImageView>& image_views() const { return image_views_; }

 private:
  const Application& app_;
  VkSwapchainKHR swapchain_;
  std::vector<VkImage> images_;
  std::vector<VkImageView> image_views_;
  VkFormat image_format_;
  VkExtent2D image_extent_;
};

extern const std::vector<const char*> kSwapChainExtensions;

} /* namespace vulkan */

#endif /* LEARNVULKAN_SWAPCHAIN_H */
